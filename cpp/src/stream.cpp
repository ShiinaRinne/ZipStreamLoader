#ifdef GBK
#include <iconv.h>
#endif

#include <spdlog/spdlog.h>
#include <cpprest/http_client.h>
#include <cpprest/streams.h>
#include <zlib.h>
#include <filesystem>
#include <optional>
#include <atomic>


struct ZipHeader {
    std::uint16_t version;
    std::uint16_t flags;
    std::uint16_t compression;
    std::uint16_t mod_time;
    std::uint16_t mod_date;
    std::uint32_t crc32_expected;
    std::uint32_t compressed_size;
    std::uint32_t uncompressed_size;
    std::uint16_t file_name_length;
    std::uint16_t extra_field_length;
};

template<typename T>
T readFromByteArray(const std::byte* &ptr) {
    T value;
    std::memcpy(&value, ptr, sizeof(T));
    ptr += sizeof(T);
    return value;
}

[[nodiscard]] ZipHeader parseHeader(const std::byte* headerData) {
    ZipHeader header;
    header.version              = readFromByteArray<std::uint16_t>(headerData);
    header.flags                = readFromByteArray<std::uint16_t>(headerData);
    header.compression          = readFromByteArray<std::uint16_t>(headerData);
    header.mod_time             = readFromByteArray<std::uint16_t>(headerData);
    header.mod_date             = readFromByteArray<std::uint16_t>(headerData);
    header.crc32_expected       = readFromByteArray<std::uint32_t>(headerData);
    header.compressed_size      = readFromByteArray<std::uint32_t>(headerData);
    header.uncompressed_size    = readFromByteArray<std::uint32_t>(headerData);
    header.file_name_length     = readFromByteArray<std::uint16_t>(headerData);
    header.extra_field_length   = readFromByteArray<std::uint16_t>(headerData);
    return header;
}


class ZipContentFetcher {
public:
    web::http::http_response currentResponse;
    std::atomic<size_t> totalSize;
    std::atomic<size_t> progress;
    std::chrono::steady_clock::time_point startTime;

    ZipContentFetcher(const std::vector<std::string>& urls) : urls(urls), currentUrlIndex(0), totalSize(0), progress(0) {
        startTime = std::chrono::steady_clock::now();
        switchStream();
    }

    void switchStream() {
        if (currentUrlIndex >= urls.size()) {
            spdlog::info("Processing complete");
            exit(0);
        }

        const auto url = urls[currentUrlIndex++];
        web::http::client::http_client client(utility::conversions::to_string_t(url));

        client.request(web::http::methods::GET, U("/")).then([this, url](const web::http::http_response response) {
            if (response.status_code() == web::http::status_codes::OK) {
                this->currentResponse = response;
                totalSize = response.headers().content_length();
                spdlog::info("Processing volume: {}", url);
                progress = 0;
                startTime = std::chrono::steady_clock::now();
            } else {
                spdlog::info("Request failed with status code: {}", response.status_code());
                switchStream();
            }
        }).wait();
    }
    
    [[nodiscard]] const std::optional<std::vector<std::byte>> read(const size_t length) {
        std::vector<std::byte> data(length);
        auto buf = currentResponse.body().streambuf();
        const auto bytesRead = buf.getn(reinterpret_cast<uint8_t*>(data.data()), length).get();
        updateProgress(bytesRead);

        if (bytesRead < length) {
            switchStream();
            auto remainingData = read(length - bytesRead);
            if (!remainingData) {
                return std::nullopt;
            }
            data.insert(data.end(), remainingData->begin(), remainingData->end());
        }

        data.resize(bytesRead);
        return data;
    }
private:
    std::vector<std::string> urls;
    size_t currentUrlIndex;

    void updateProgress(const size_t bytesRead) noexcept {
        progress += bytesRead;
        displayProgress();
    }

    void displayProgress() noexcept {
        auto now = std::chrono::steady_clock::now();
        auto timeElapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
        auto speed = timeElapsed > 0 ? (progress / 1024.0 / 1024.0) / timeElapsed : 0; // MB/s

        int barWidth = 70;
        float progressPercentage = totalSize > 0 ? (float)progress / totalSize : 0.0;
        std::cout << "[";
        int pos = barWidth * progressPercentage;
        for (int i = 0; i < barWidth; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << std::setprecision(2) << int(progressPercentage * 100.0) << " % " << speed << " MB/s\r";
        std::cout.flush();
    }
};

#ifdef GBK
[[nodiscard]] std::string convertBytesToString(const std::vector<std::byte>& gbkData) {
    size_t inBytes = gbkData.size();
    char* inputPtr = const_cast<char*>(reinterpret_cast<const char*>(gbkData.data()));
    
    size_t outBytes = 4 * gbkData.size();
    std::vector<char> utf8Data(outBytes);
    char* outputPtr = utf8Data.data();
    
    const iconv_t cd = iconv_open("UTF-8", "GBK");
    if (cd == (iconv_t)-1) {
        throw std::runtime_error("Failed to open iconv");
    }
    
    const size_t result = iconv(cd, &inputPtr, &inBytes, &outputPtr, &outBytes);
    if (result == (size_t)-1) {
        iconv_close(cd);
        throw std::runtime_error("iconv conversion failed");
    }
    
    iconv_close(cd);
    
    return std::string(utf8Data.data(), utf8Data.size() - outBytes);
}
#else
[[nodiscard]] std::string convertBytesToString(const std::vector<std::byte>& byteData) {
    std::string str(byteData.size(), '\0');
    for (size_t i = 0; i < byteData.size(); ++i) {
        str[i] = static_cast<char>(byteData[i]);
    }

    return str;
}
#endif

void decompressDeflate(const std::vector<std::byte>& compressedData, std::vector<std::byte>& decompressedData, const uLongf decompressedSize) {
    z_stream strm{};
    strm.next_in = (Bytef*)compressedData.data();
    strm.avail_in = compressedData.size();
    strm.next_out = (Bytef*)decompressedData.data();
    strm.avail_out = decompressedSize;

    if (int ret = inflateInit2(&strm, -MAX_WBITS); ret != Z_OK) {
        throw std::runtime_error("inflateInit2 failed");
    }

    auto inflater = std::unique_ptr<z_stream, decltype(&inflateEnd)>(&strm, &inflateEnd);

    if (int ret = inflate(inflater.get(), Z_FINISH); ret != Z_STREAM_END) {
        throw std::runtime_error("inflate failed: " + std::to_string(ret));
    }

    decompressedData.resize(inflater->total_out);
}

void decompressData(const ZipHeader& header, const std::vector<std::byte>& compressedData, const std::string& outputFilePath) {
    std::vector<std::byte> decompressedData(header.uncompressed_size);
    
    switch(header.compression) {
        case 0: { // No Compression (Stored)
            spdlog::debug("Decompressing: {}. Compression Type: {}", outputFilePath, "No Compression (Stored)");
            decompressedData = compressedData;
            break;
        }
        case 8: { // Deflate
            spdlog::debug("Decompressing: {}. Compression type: {}", outputFilePath, "Deflate");
            decompressDeflate(compressedData, decompressedData, header.uncompressed_size);
            break;
        }
        default:
            throw std::runtime_error("Unsupported compression format.");
    }

    if (header.crc32_expected != 0) {
        const uLong calculatedCrc = crc32(0L, reinterpret_cast<const Bytef*>(decompressedData.data()), decompressedData.size());
        if (calculatedCrc != header.crc32_expected) {
            throw std::runtime_error("CRC32 check failed");
        }
    }

    std::ofstream outFile(outputFilePath, std::ios::binary);
    if (!outFile.write(reinterpret_cast<const char*>(decompressedData.data()), decompressedData.size())) {
        throw std::runtime_error("Failed to write decompressed data to file.");
    }
}

void decompressTask(const ZipHeader& header, const std::vector<std::byte>& compressedData, const std::string& outputFilePath) noexcept {
    try {
        decompressData(header, compressedData, outputFilePath);
    } catch (const std::exception& e) {
        spdlog::error("Error occurred: {}", e.what());
    }
}

void downloadAndDecompress(const std::vector<std::string>& zipList, const std::string& outpputDir) {
    ZipContentFetcher fetcher(zipList);
    std::vector<std::jthread> threads;
    
    while (true) {
        const auto signature = fetcher.read(4);
        if (signature->empty() || signature->size() != 4|| memcmp(signature->data(), "PK\x01\x02", 4) == 0) {
            spdlog::info("Finished reading file");
            break;
        }
        
        const auto header_data = fetcher.read(26);
        if (header_data->empty() || header_data->size() != 26) {
            spdlog::error("Error: Failed to read header data");
            break;
        }

        const ZipHeader header     = parseHeader(header_data->data());
        const auto fileNameData    = fetcher.read(header.file_name_length);
        const auto extra_field     = fetcher.read(header.extra_field_length);
        const auto compressData    = fetcher.read(header.compressed_size);
        const std::string fileName = convertBytesToString(*fileNameData);

        if (!compressData || compressData->size() != header.compressed_size) {
            spdlog::error("Error: Failed to read compressed data for file: {}", fileName);
            break;
        }

        const auto outputPath = std::filesystem::path(outpputDir) / fileName;
        if (fileName.back() == '/') {
            std::filesystem::create_directories(outputPath);
            continue;
        }
        
        threads.emplace_back([=, &fetcher]() {
            decompressTask(header, *compressData, outputPath.string());
        });
    }
}

[[nodiscard]] const std::string getCmdOption(const char** begin, const char** end, const std::string& longOption, const std::string& shortOption = "", const std::string& defaultValue = "") {
    auto itr = std::find_if(begin, end, [&](const char* arg) {
        return longOption == arg || (!shortOption.empty() && shortOption == arg);
    });

    if (itr != end && ++itr != end) {
        return *itr;
    }

    return defaultValue;
}

[[nodiscard]] bool cmdOptionExists(const char** begin, const char** end, const std::string& longOption, const std::string& shortOption = "") {
    return std::find_if(begin, end, [&](const char* arg) {
        return longOption == arg || (!shortOption.empty() && shortOption == arg);
    }) != end;
}


int main(int argc, const char* argv[]) {
    bool showHelp = cmdOptionExists(argv, argv+argc, "--help", "-h");
    if(showHelp){
        std::cout << 
R"(Zip Stream Downloader
usage: stream [-h] [--log-level {INFO,WARNING,ERROR,CRITICAL}] [--output-dir OUTPUT_DIR] [--urls URLS [URLS ...]]
        [--disable-crc32]

options:
  -h, --help            show this help message and exit
  --log-level {INFO,WARNING,ERROR,CRITICAL}
                        Sets the log level, default is `INFO`.
  --output-dir OUTPUT_DIR
                        Sets the download directory, default is `./output`.
  --urls URLS [URLS ...]
                        The list of URLs to download.
)" << std::endl;
        return 0;
    }

    const auto   logLevel = getCmdOption(argv, argv + argc, "--log-level", "-l", "INFO");
    if        (  logLevel == "DEBUG"    )   { spdlog::set_level(spdlog::level::debug)   ;}
    else if   (  logLevel == "INFO"     )   { spdlog::set_level(spdlog::level::info)    ;} 
    else if   (  logLevel == "WARNING"  )   { spdlog::set_level(spdlog::level::warn)    ;} 
    else if   (  logLevel == "ERROR"    )   { spdlog::set_level(spdlog::level::err)     ;} 
    else if   (  logLevel == "CRITICAL" )   { spdlog::set_level(spdlog::level::critical);} 
    else                                    { spdlog::warn("Unknown log level: {}", logLevel) ;}
    
    const std::string outputDir   = getCmdOption(argv, argv + argc, "--output-dir", "-o", "./output/");
    
    std::vector<std::string> urls;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--urls" || arg == "-u") {
            while (i + 1 < argc && argv[i + 1][0] != '-') {
                urls.push_back(argv[++i]);
            }
        }
    }

    if (urls.empty()) {
        spdlog::error("No URLs provided");
        return 1;
    }

    if(!std::filesystem::exists(outputDir)) {
        std::filesystem::create_directories(outputDir);
    }

    try {
        downloadAndDecompress(urls, outputDir);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << std::endl;
    }
    
    return 0;
}

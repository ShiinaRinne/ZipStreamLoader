#include "spdlog/spdlog.h"
#include <iconv.h>
#include <cpprest/http_client.h>
#include <cpprest/streams.h>
#include <zlib.h>
#include <filesystem>

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

ZipHeader parseHeader(const std::byte* headerData) {
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

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::vector<std::byte>* userp) {
    auto realSize = size * nmemb;
    const std::byte* start = static_cast<const std::byte*>(contents);
    userp->insert(userp->end(), start, start + realSize);
    return realSize;
}

class ZipContentFetcher {
public:
    web::http::http_response currentResponse;

    ZipContentFetcher(const std::vector<std::string>& urls) : urls(urls), currentUrlIndex(0) {
        switchStream();
    }

    void switchStream() {
        if (currentUrlIndex >= urls.size()) {
            spdlog::info("Processing complete");
            exit(0);
        }

        auto url = urls[currentUrlIndex++];
        web::http::client::http_client client(utility::conversions::to_string_t(url));

        client.request(web::http::methods::GET, U("/")).then([this, url](web::http::http_response response) {
            if (response.status_code() == web::http::status_codes::OK) {
                this->currentResponse = response;
                spdlog::info("Processing volume: {}", url);
            } else {
                spdlog::info("Request failed with status code: {}", response.status_code());
                switchStream();
            }
        }).wait();
    }
    
    std::shared_ptr<std::vector<std::byte>> read(size_t length) {
        auto data = std::make_shared<std::vector<std::byte>>(length);
    
        auto buf = currentResponse.body().streambuf();
        auto bytesRead = buf.getn(reinterpret_cast<uint8_t*>(data->data()), length).get();
    
        data->resize(bytesRead);
    
        if (bytesRead < length) {
            switchStream();
            auto remainingData = read(length - bytesRead);
            data->insert(data->end(), remainingData->begin(), remainingData->end());
        }
    
        return data;
    }
private:
    std::vector<std::string> urls;
    size_t currentUrlIndex;
    
};



std::string convertGBKToUTF8(const std::vector<std::byte>& gbkData) {
    size_t inBytes = gbkData.size();
    char* inputPtr = const_cast<char*>(reinterpret_cast<const char*>(gbkData.data()));
    
    size_t outBytes = 4 * gbkData.size();
    std::vector<char> utf8Data(outBytes);
    char* outputPtr = utf8Data.data();
    
    iconv_t cd = iconv_open("UTF-8", "GBK");
    if (cd == (iconv_t)-1) {
        throw std::runtime_error("Failed to open iconv");
    }
    
    size_t result = iconv(cd, &inputPtr, &inBytes, &outputPtr, &outBytes);
    if (result == (size_t)-1) {
        iconv_close(cd);
        throw std::runtime_error("iconv conversion failed");
    }
    
    iconv_close(cd);
    
    return std::string(utf8Data.data(), utf8Data.size() - outBytes);
}

void decompressDeflate(const std::vector<std::byte>& compressedData, std::vector<std::byte>& decompressedData, uLongf decompressedSize) {
    z_stream strm = {};
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = compressedData.size();
    strm.next_in = (Bytef*)compressedData.data();
    
    if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) {
        throw std::runtime_error("inflateInit2 failed");
    }
    
    int ret;
    strm.avail_out = decompressedSize;
    strm.next_out = (Bytef*)decompressedData.data();
    ret = inflate(&strm, Z_NO_FLUSH);
    inflateEnd(&strm);

    if (ret != Z_STREAM_END) {
        throw std::runtime_error("inflate failed: " + std::to_string(ret));
    }
}

void decompressData(const ZipHeader& header, const std::vector<std::byte>& compressedData, const std::string& outputFilePath) {
    
    std::vector<std::byte> decompressedData(header.uncompressed_size);
    
    switch(header.compression) {
        case 0: // No Compression (Stored)
            spdlog::debug("Decompressing: {}. Compression Type: {}", outputFilePath, "No Compression (Stored)");
            decompressedData = compressedData;
            break;
        case 8: { // Deflate
            spdlog::debug("Decompressing: {}. Compression type: {}", outputFilePath, "Deflate");
            decompressDeflate(compressedData, decompressedData, header.uncompressed_size);
            break;
        }
        default:
            throw std::runtime_error("Unsupported compression format.");
    }

    if (header.crc32_expected != 0) {
        uLong calculatedCrc = crc32(0L, Z_NULL, 0);
        calculatedCrc = crc32(calculatedCrc, reinterpret_cast<const Bytef*>(decompressedData.data()), decompressedData.size());
        if (calculatedCrc != header.crc32_expected) {
            throw std::runtime_error("CRC32 check failed");
        }
    }

    std::ofstream outFile(outputFilePath, std::ios::binary);
    if (!outFile.write(reinterpret_cast<const char*>(decompressedData.data()), decompressedData.size())) {
        throw std::runtime_error("Failed to write decompressed data to file.");
    }
}

void decompressTask(const ZipHeader& header, const std::vector<std::byte>& compressedData, const std::string& outputFilePath) {
    try {
        decompressData(header, compressedData, outputFilePath);
    } catch (const std::exception& e) {
        spdlog::error("Error occurred: {}", e.what());
    }
}


void downloadAndDecompress(const std::vector<std::string>& zipList,const std::string& outpputDir, const bool& disableCrc32) {
    ZipContentFetcher fetcher(zipList);
    std::vector<std::jthread> threads;
    
    while (true) {
        auto signature = fetcher.read(4);
        if (signature->empty() || signature->size() != 4|| memcmp(signature->data(), "PK\x01\x02", 4) == 0) {
            spdlog::info("Finished reading file");
            break;
        }
        
        auto header_data = fetcher.read(26);
        if (header_data->empty() || header_data->size() != 26) {
            spdlog::error("Error: Failed to read header data");
            break;
        }
        auto header = parseHeader(header_data->data());
        auto fileNameData = fetcher.read(header.file_name_length);
        auto fileName = convertGBKToUTF8(*fileNameData);
        auto crc = header.crc32_expected;
        
        auto extra_field = fetcher.read(header.extra_field_length);
        
        
        auto compressData = fetcher.read(header.compressed_size);
        if (!compressData || compressData->size() != header.compressed_size) {
            spdlog::error("Error: Failed to read compressed data for file: {}", fileName);
            break;
        }
        if(!std::filesystem::exists(outpputDir)) {
            std::filesystem::create_directories(outpputDir);
        }
        std::filesystem::path outputPath = std::filesystem::path(outpputDir) / fileName;
        if (fileName.back() == '/') {
            std::filesystem::create_directories(outputPath);
            continue;
        }
        
        threads.emplace_back([=, &fetcher]() {
            decompressTask(header, *compressData, outputPath.string());
        });
    }
}

std::string getCmdOption(char ** begin, char ** end, const std::string & longOption, const std::string & shortOption = "", const std::string & defaultValue = "") {
    auto itr = std::find_if(begin, end, [&](const char* arg) {
        return longOption == arg || (!shortOption.empty() && shortOption == arg);
    });
    if (itr != end && ++itr != end) {
        return *itr;
    }
    return defaultValue;
}

bool cmdOptionExists(char** begin, char** end, const std::string& longOption, const std::string& shortOption = "") {
    return std::find_if(begin, end, [&](const char* arg) {
        return longOption == arg || (!shortOption.empty() && shortOption == arg);
    }) != end;
}



int main(int argc, char * argv[]) {
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
  --disable-crc32       Disables CRC32 check, off by default.
)" << std::endl;
        return 0;
    }

    
    std::string  logLevel = getCmdOption(argv, argv + argc, "--log-level", "-l", "INFO");
    if        (  logLevel == "INFO"     )   { spdlog::set_level(spdlog::level::info);} 
    else if   (  logLevel == "WARNING"  )   { spdlog::set_level(spdlog::level::warn);} 
    else if   (  logLevel == "ERROR"    )   { spdlog::set_level(spdlog::level::err);} 
    else if   (  logLevel == "CRITICAL" )   { spdlog::set_level(spdlog::level::critical);} 
    else                                    { spdlog::warn("Unknown log level: {}", logLevel);}
    

    bool disableCrc32       = cmdOptionExists(argv, argv + argc, "--disable-crc32");
    std::string outputDir   = getCmdOption(argv, argv + argc, "--output-dir", "-o", "./output/");
    std::vector<std::string> urls;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--urls" || arg == "-u") {
            while (i + 1 < argc && argv[i + 1][0] != '-') {
                urls.push_back(argv[++i]);
            }
        }
    }


    try {
        downloadAndDecompress(urls, outputDir, disableCrc32);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << std::endl;
    }
    
    return 0;
}

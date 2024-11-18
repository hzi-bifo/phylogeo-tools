#ifndef __TARXZ_READER_H__
#define __TARXZ_READER_H__
#include <archive.h>
#include <archive_entry.h>
#include <lzma.h>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>

class TarXZReader {
private:
    struct archive *a = nullptr;
    std::string line_buffer;
    bool eof = true;

public:
    la_int64_t current_file_size = 0;
    la_int64_t current_file_offset = 0;

    TarXZReader() = default;

    ~TarXZReader() {
        close();
    }

    // Open the .tar.xz file
    bool open(const std::string &filename) {
        eof = true;
        a = archive_read_new();
        archive_read_support_format_all(a);
        archive_read_support_filter_all(a);

        int r;
        if ((r = archive_read_open_filename(a, filename.c_str(), 10240))) {
            std::cerr << "Failed to open file " << filename << std::endl;
            return false;
        }
        return true;
    }

    // Close all resources
    void close() {
        if (a != nullptr) {
            archive_read_close(a);
            archive_read_free(a);
            a = nullptr;
        }
    }

    // Seek into a specific file inside the archive
    bool seekFile(const std::string &filePath) {
        struct archive_entry *entry;

        for (;;) {
            int r = archive_read_next_header(a, &entry);
            if (r == ARCHIVE_EOF)
                break;
            if (r < ARCHIVE_OK) {
                std::cerr << archive_error_string(a) << std::endl;
                return false;
            }
            if (r < ARCHIVE_WARN) {
                std::cerr << "W: " << archive_error_string(a) << std::endl;
                return false;
            }
            if (archive_entry_size(entry) > 0) {
                current_file_size = archive_entry_size(entry);
                // std::cerr << archive_entry_pathname(entry) << " " << "size=" << current_file_size << std::endl;
                if (archive_entry_pathname(entry) == filePath) {
                    eof = false;
                    return true;
                }
            }
        }
        return false;
    }

    // Read the next line from the currently selected file
    bool getline(std::string &line) {

        for (; !eof;) {
            size_t pos = line_buffer.find('\n');
            if (pos != std::string::npos) {
                line = line_buffer.substr(0, pos);
                line_buffer.erase(0, pos + 1);
                return true;
            }

            const void *buff;
            size_t size;
            la_int64_t offset;
            int r = archive_read_data_block(a, &buff, &size, &offset);
            current_file_offset = offset;
            // std::cerr << "R " << size << " " << offset << " " << r << std::endl;
            if (r == ARCHIVE_OK && size > 0) {
                line_buffer.append((char*)buff, size);
                continue;
            }
            if (r == ARCHIVE_EOF || (r == ARCHIVE_OK && size == 0)) {
                // std::cerr << "R-EOF '" << line_buffer << "'" << std::endl;
                eof = true;
                // line = line_buffer;
                // line_buffer = "";
                // return true;
                return false;
            }
            if (r < ARCHIVE_OK) {
                std::cerr << "Error: " << r << std::endl;
                return false;
            }
        }

        return false;
    }
};

bool getline(TarXZReader& is, std::string& line) {
    return is.getline(line);
}

#endif

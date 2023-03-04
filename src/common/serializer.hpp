#pragma once

#include <array>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <queue>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Used for save states and configuration files; reads or writes data from/to files
class Serializer {
public:
    enum class Mode {
        Read,
        Write
    };

    Serializer(Mode mode) : has_error_(false), is_open_(false), mode_(mode) {}
    Serializer(Mode mode, std::filesystem::path const& path) : mode_(mode) { open(path); }
    Serializer(Serializer const&) = delete;
    Serializer(Serializer&& other) noexcept { *this = std::move(other); }
    Serializer& operator=(Serializer const&) = delete;
    Serializer& operator=(Serializer&& other) noexcept
    {
        if (this != &other) {
            fstream_.swap(other.fstream_);
            is_open_ = other.is_open_;
            has_error_ = other.has_error_;
            mode_ = other.mode_;
            other.close();
        }
        return *this;
    }

    void close()
    {
        if (is_open_) {
            fstream_.close();
            is_open_ = false;
        }
        has_error_ = false;
    }

    bool has_error() const { return has_error_; }

    void open(std::filesystem::path const& path)
    {
        if (is_open_) {
            fstream_.close();
        }
        auto ios_base = std::ios::binary | (mode_ == Mode::Read ? std::ios::in : std::ios::out | std::ios::trunc);
        fstream_.open(path, ios_base);
        is_open_ = !!fstream_;
        has_error_ = !fstream_;
    }

    template<typename T>
    void stream_deque(std::deque<T>& deque)
        requires(std::is_trivially_copyable_v<T>)
    {
        if (mode_ == Mode::Read) {
            deque.clear();
            size_t size;
            stream(&size, sizeof(size_t));
            if (size > 0) {
                deque.reserve(size);
            }
            for (size_t i = 0; i < size; i++) {
                T t;
                stream(&t, sizeof(T));
                deque.push_back(t);
            }
        } else {
            size_t size = deque.size();
            stream(&size, sizeof(size_t));
            for (T& t : deque) {
                stream(&t, sizeof(T));
            }
        }
    }

    template<typename T>
    void stream_queue(std::queue<T>& queue)
        requires(std::is_trivially_copyable_v<T>)
    {
        if (mode_ == Mode::Read) {
            while (!queue.empty()) {
                queue.pop();
            }
            size_t size;
            stream(&size, sizeof(size_t));
            for (size_t i = 0; i < size; i++) {
                T t;
                stream(&t, sizeof(T));
                queue.push(t);
            }
        } else {
            size_t size = queue.size();
            stream(&size, sizeof(size_t));
            auto tmp_queue = queue;
            while (!tmp_queue.empty()) {
                T t = tmp_queue.front();
                tmp_queue.pop();
                stream(&t, sizeof(T));
            }
        }
    }

    void stream_string(std::string& str)
    {
        if (mode_ == Mode::Read) {
            size_t size;
            stream(&size, sizeof(size_t));
            char* c_str = new char[size + 1]{};
            stream(c_str, size * sizeof(char));
            str = std::string(c_str);
            delete[] c_str;
        } else {
            char const* c_str = str.c_str();
            size_t size = std::strlen(c_str);
            stream(&size, sizeof(size_t));
            stream((void*)c_str, size * sizeof(char));
        }
    }

    template<typename T>
    void stream_trivial(T& val)
        requires(std::is_trivially_copyable_v<T>)
    {
        stream(&val, sizeof(T));
    }

    template<typename T>
    void stream_vector(std::vector<T>& vec)
        requires(std::is_trivially_copyable_v<T>)
    {
        if (mode_ == Mode::Read) {
            vec.clear();
            size_t size = 0;
            stream(&size, sizeof(size_t));
            if (size > 0) {
                vec.reserve(size);
            }
            for (size_t i = 0; i < size; i++) {
                T t;
                stream(&t, sizeof(T));
                vec.push_back(t);
            }
        } else {
            size_t size = vec.size();
            stream(&size, sizeof(size_t));
            for (T& t : vec) {
                stream(&t, sizeof(T));
            }
        }
    }

private:
    bool has_error_;
    bool is_open_;
    std::fstream fstream_;
    Mode mode_;

    void stream(void* obj, size_t size)
    {
        if (has_error_) {
            return;
        }
        if (mode_ == Mode::Read) {
            fstream_.read(reinterpret_cast<char*>(obj), size);
        } else {
            fstream_.write(reinterpret_cast<char const*>(obj), size);
        }
        if (!fstream_) {
            has_error_ = true; // todo; implement better error handling
        }
    }
};

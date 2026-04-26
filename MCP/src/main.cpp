#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{

static const char* kProtocolVersion = "2025-03-26";
static const int kResourceNotFound = -32002;

std::string NarrowFromWide(const std::wstring& value)
{
    if (value.empty())
        return std::string();

    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                                         nullptr, 0, nullptr, nullptr);
    if (size <= 0)
        return std::string();

    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                        &out[0], size, nullptr, nullptr);
    return out;
}

std::wstring WideFromUtf8(const std::string& value)
{
    if (value.empty())
        return std::wstring();

    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                                         nullptr, 0);
    if (size <= 0)
        return std::wstring();

    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                        &out[0], size);
    return out;
}

bool StartsWithI(const std::wstring& value, const std::wstring& prefix)
{
    if (value.size() < prefix.size())
        return false;
    for (size_t i = 0; i < prefix.size(); ++i)
    {
        if (towlower(value[i]) != towlower(prefix[i]))
            return false;
    }
    return true;
}

std::wstring NormalizeSlashes(std::wstring path)
{
    std::replace(path.begin(), path.end(), L'/', L'\\');
    return path;
}

std::wstring EnsureTrailingSlash(std::wstring value)
{
    if (!value.empty() && value.back() != L'\\' && value.back() != L'/')
        value.push_back(L'\\');
    return value;
}

bool IsAlphaNumAscii(unsigned char ch)
{
    return (ch >= 'A' && ch <= 'Z') ||
           (ch >= 'a' && ch <= 'z') ||
           (ch >= '0' && ch <= '9');
}

std::string PercentEncodeUtf8(const std::string& value)
{
    std::ostringstream out;
    out << std::uppercase << std::hex;
    for (size_t i = 0; i < value.size(); ++i)
    {
        const unsigned char ch = static_cast<unsigned char>(value[i]);
        if (IsAlphaNumAscii(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '/' || ch == '~')
        {
            out << static_cast<char>(ch);
        }
        else
        {
            out << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
        }
    }
    return out.str();
}

std::string PercentDecodeUtf8(const std::string& value)
{
    std::string out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] == '%' && i + 2 < value.size())
        {
            const char hi = value[i + 1];
            const char lo = value[i + 2];
            if (std::isxdigit(static_cast<unsigned char>(hi)) &&
                std::isxdigit(static_cast<unsigned char>(lo)))
            {
                const std::string hex = value.substr(i + 1, 2);
                const int decoded = std::strtol(hex.c_str(), nullptr, 16);
                out.push_back(static_cast<char>(decoded));
                i += 2;
                continue;
            }
        }
        out.push_back(value[i]);
    }
    return out;
}

std::wstring CanonicalizePath(const std::wstring& path)
{
    std::wstring normalized = NormalizeSlashes(path);
    DWORD required = GetFullPathNameW(normalized.c_str(), 0, nullptr, nullptr);
    if (required == 0)
        return std::wstring();

    std::wstring full(static_cast<size_t>(required), L'\0');
    LPWSTR filePart = nullptr;
    DWORD written = GetFullPathNameW(normalized.c_str(), required, &full[0], &filePart);
    if (written == 0)
        return std::wstring();

    if (!full.empty() && full.back() == L'\0')
        full.pop_back();

    return NormalizeSlashes(full);
}

bool PathExists(const std::wstring& path)
{
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES;
}

bool IsDirectoryPath(const std::wstring& path)
{
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool IsWithinRoot(const std::wstring& root, const std::wstring& path)
{
    std::wstring fullRoot = EnsureTrailingSlash(root);
    if (StartsWithI(path, fullRoot))
        return true;
    return _wcsicmp(root.c_str(), path.c_str()) == 0;
}

std::wstring JoinPath(const std::wstring& base, const std::wstring& leaf)
{
    if (base.empty())
        return leaf;
    if (leaf.empty())
        return base;
    if (base.back() == L'\\' || base.back() == L'/')
        return base + leaf;
    return base + L"\\" + leaf;
}

std::string GuessMimeType(const std::wstring& path, bool isDirectory)
{
    if (isDirectory)
        return "inode/directory";

    std::wstring ext;
    const size_t dot = path.find_last_of(L'.');
    if (dot != std::wstring::npos)
        ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), towlower);

    if (ext == L".txt" || ext == L".md" || ext == L".json" || ext == L".yml" || ext == L".yaml" ||
        ext == L".xml" || ext == L".ini" || ext == L".log" || ext == L".ps1" || ext == L".cmake")
        return "text/plain";
    if (ext == L".cpp" || ext == L".h" || ext == L".hpp" || ext == L".cxx" || ext == L".cc")
        return "text/x-c++src";
    if (ext == L".c")
        return "text/x-csrc";
    if (ext == L".hlsl")
        return "text/plain";
    if (ext == L".png")
        return "image/png";
    if (ext == L".jpg" || ext == L".jpeg")
        return "image/jpeg";
    if (ext == L".gif")
        return "image/gif";
    if (ext == L".dds")
        return "image/vnd-ms.dds";
    if (ext == L".pdf")
        return "application/pdf";
    if (ext == L".toml")
        return "application/toml";
    return "application/octet-stream";
}

bool IsLikelyTextMime(const std::string& mimeType)
{
    return mimeType.find("text/") == 0 ||
           mimeType == "application/json" ||
           mimeType == "application/toml" ||
           mimeType == "application/xml" ||
           mimeType == "text/x-c++src" ||
           mimeType == "text/x-csrc";
}

std::string Base64Encode(const std::vector<uint8_t>& data)
{
    static const char* kChars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);

    for (size_t i = 0; i < data.size(); i += 3)
    {
        const uint32_t b0 = data[i];
        const uint32_t b1 = (i + 1 < data.size()) ? data[i + 1] : 0;
        const uint32_t b2 = (i + 2 < data.size()) ? data[i + 2] : 0;
        const uint32_t triple = (b0 << 16) | (b1 << 8) | b2;

        out.push_back(kChars[(triple >> 18) & 0x3F]);
        out.push_back(kChars[(triple >> 12) & 0x3F]);
        out.push_back(i + 1 < data.size() ? kChars[(triple >> 6) & 0x3F] : '=');
        out.push_back(i + 2 < data.size() ? kChars[triple & 0x3F] : '=');
    }

    return out;
}

struct JsonValue
{
    enum class Type
    {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    Type type = Type::Null;
    bool boolValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    std::vector<JsonValue> arrayValue;
    std::map<std::string, JsonValue> objectValue;

    static JsonValue MakeNull() { return JsonValue(); }
    static JsonValue MakeBool(bool value) { JsonValue out; out.type = Type::Bool; out.boolValue = value; return out; }
    static JsonValue MakeNumber(double value) { JsonValue out; out.type = Type::Number; out.numberValue = value; return out; }
    static JsonValue MakeString(const std::string& value) { JsonValue out; out.type = Type::String; out.stringValue = value; return out; }
    static JsonValue MakeArray() { JsonValue out; out.type = Type::Array; return out; }
    static JsonValue MakeObject() { JsonValue out; out.type = Type::Object; return out; }

    bool IsNull() const { return type == Type::Null; }
    bool IsString() const { return type == Type::String; }
    bool IsObject() const { return type == Type::Object; }
    bool IsArray() const { return type == Type::Array; }
    bool IsNumber() const { return type == Type::Number; }

    const JsonValue* Find(const std::string& key) const
    {
        if (type != Type::Object)
            return nullptr;
        std::map<std::string, JsonValue>::const_iterator it = objectValue.find(key);
        return it == objectValue.end() ? nullptr : &it->second;
    }
};

struct JsonParser
{
    explicit JsonParser(const std::string& textIn) : text(textIn), pos(0) {}

    const std::string& text;
    size_t pos;
    std::string error;

    void SkipWs()
    {
        while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\r' || text[pos] == '\n'))
            ++pos;
    }

    bool Parse(JsonValue& out);
    bool ParseValue(JsonValue& out);
    bool ParseLiteral(const char* literal, const JsonValue& value, JsonValue& out);
    bool ParseStringRaw(std::string& out);
    bool ParseStringValue(JsonValue& out);
    bool ParseNumber(JsonValue& out);
    bool ParseArray(JsonValue& out);
    bool ParseObject(JsonValue& out);
};

bool JsonParser::Parse(JsonValue& out)
{
    SkipWs();
    if (!ParseValue(out))
        return false;
    SkipWs();
    if (pos != text.size())
    {
        error = "trailing characters";
        return false;
    }
    return true;
}

bool JsonParser::ParseValue(JsonValue& out)
{
    SkipWs();
    if (pos >= text.size())
    {
        error = "unexpected end of input";
        return false;
    }

    const char ch = text[pos];
    if (ch == 'n') return ParseLiteral("null", JsonValue::MakeNull(), out);
    if (ch == 't') return ParseLiteral("true", JsonValue::MakeBool(true), out);
    if (ch == 'f') return ParseLiteral("false", JsonValue::MakeBool(false), out);
    if (ch == '"') return ParseStringValue(out);
    if (ch == '{') return ParseObject(out);
    if (ch == '[') return ParseArray(out);
    if (ch == '-' || (ch >= '0' && ch <= '9')) return ParseNumber(out);

    error = "invalid token";
    return false;
}

bool JsonParser::ParseLiteral(const char* literal, const JsonValue& value, JsonValue& out)
{
    const size_t len = std::strlen(literal);
    if (text.compare(pos, len, literal) != 0)
    {
        error = "invalid literal";
        return false;
    }
    pos += len;
    out = value;
    return true;
}

bool JsonParser::ParseStringRaw(std::string& out)
{
    if (pos >= text.size() || text[pos] != '"')
    {
        error = "expected string";
        return false;
    }

    ++pos;
    out.clear();
    while (pos < text.size())
    {
        const char ch = text[pos++];
        if (ch == '"')
            return true;
        if (ch == '\\')
        {
            if (pos >= text.size())
            {
                error = "bad escape";
                return false;
            }

            const char esc = text[pos++];
            switch (esc)
            {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u':
                error = "unicode escapes are not supported in requests";
                return false;
            default:
                error = "invalid escape";
                return false;
            }
        }
        else
        {
            out.push_back(ch);
        }
    }

    error = "unterminated string";
    return false;
}

bool JsonParser::ParseStringValue(JsonValue& out)
{
    std::string value;
    if (!ParseStringRaw(value))
        return false;
    out = JsonValue::MakeString(value);
    return true;
}

bool JsonParser::ParseNumber(JsonValue& out)
{
    const size_t start = pos;
    if (text[pos] == '-')
        ++pos;
    if (pos >= text.size())
    {
        error = "bad number";
        return false;
    }
    if (text[pos] == '0')
    {
        ++pos;
    }
    else
    {
        if (!std::isdigit(static_cast<unsigned char>(text[pos])))
        {
            error = "bad number";
            return false;
        }
        while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])))
            ++pos;
    }
    if (pos < text.size() && text[pos] == '.')
    {
        ++pos;
        if (pos >= text.size() || !std::isdigit(static_cast<unsigned char>(text[pos])))
        {
            error = "bad number";
            return false;
        }
        while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])))
            ++pos;
    }
    if (pos < text.size() && (text[pos] == 'e' || text[pos] == 'E'))
    {
        ++pos;
        if (pos < text.size() && (text[pos] == '+' || text[pos] == '-'))
            ++pos;
        if (pos >= text.size() || !std::isdigit(static_cast<unsigned char>(text[pos])))
        {
            error = "bad exponent";
            return false;
        }
        while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])))
            ++pos;
    }

    const std::string numberText = text.substr(start, pos - start);
    char* endPtr = nullptr;
    const double value = std::strtod(numberText.c_str(), &endPtr);
    if (endPtr == nullptr || *endPtr != '\0')
    {
        error = "failed to parse number";
        return false;
    }

    out = JsonValue::MakeNumber(value);
    return true;
}

bool JsonParser::ParseArray(JsonValue& out)
{
    if (text[pos] != '[')
    {
        error = "expected array";
        return false;
    }

    ++pos;
    JsonValue result = JsonValue::MakeArray();
    SkipWs();
    if (pos < text.size() && text[pos] == ']')
    {
        ++pos;
        out = result;
        return true;
    }

    while (true)
    {
        JsonValue item;
        if (!ParseValue(item))
            return false;
        result.arrayValue.push_back(item);

        SkipWs();
        if (pos >= text.size())
        {
            error = "unterminated array";
            return false;
        }
        if (text[pos] == ']')
        {
            ++pos;
            out = result;
            return true;
        }
        if (text[pos] != ',')
        {
            error = "expected comma";
            return false;
        }
        ++pos;
    }
}

bool JsonParser::ParseObject(JsonValue& out)
{
    if (text[pos] != '{')
    {
        error = "expected object";
        return false;
    }

    ++pos;
    JsonValue result = JsonValue::MakeObject();
    SkipWs();
    if (pos < text.size() && text[pos] == '}')
    {
        ++pos;
        out = result;
        return true;
    }

    while (true)
    {
        SkipWs();
        std::string key;
        if (!ParseStringRaw(key))
            return false;
        SkipWs();
        if (pos >= text.size() || text[pos] != ':')
        {
            error = "expected colon";
            return false;
        }
        ++pos;

        JsonValue value;
        if (!ParseValue(value))
            return false;

        result.objectValue[key] = value;

        SkipWs();
        if (pos >= text.size())
        {
            error = "unterminated object";
            return false;
        }
        if (text[pos] == '}')
        {
            ++pos;
            out = result;
            return true;
        }
        if (text[pos] != ',')
        {
            error = "expected comma";
            return false;
        }
        ++pos;
    }
}

std::string JsonEscape(const std::string& value)
{
    std::ostringstream out;
    for (size_t i = 0; i < value.size(); ++i)
    {
        const unsigned char ch = static_cast<unsigned char>(value[i]);
        switch (ch)
        {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (ch < 0x20)
            {
                out << "\\u"
                    << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
                    << static_cast<int>(ch)
                    << std::dec;
            }
            else
            {
                out << static_cast<char>(ch);
            }
            break;
        }
    }
    return out.str();
}

std::string JsonToString(const JsonValue& value)
{
    switch (value.type)
    {
    case JsonValue::Type::Null:
        return "null";
    case JsonValue::Type::Bool:
        return value.boolValue ? "true" : "false";
    case JsonValue::Type::Number:
        {
            std::ostringstream out;
            out << std::setprecision(15) << value.numberValue;
            return out.str();
        }
    case JsonValue::Type::String:
        return "\"" + JsonEscape(value.stringValue) + "\"";
    case JsonValue::Type::Array:
        {
            std::ostringstream out;
            out << "[";
            for (size_t i = 0; i < value.arrayValue.size(); ++i)
            {
                if (i != 0)
                    out << ",";
                out << JsonToString(value.arrayValue[i]);
            }
            out << "]";
            return out.str();
        }
    case JsonValue::Type::Object:
        {
            std::ostringstream out;
            out << "{";
            bool first = true;
            for (std::map<std::string, JsonValue>::const_iterator it = value.objectValue.begin();
                 it != value.objectValue.end(); ++it)
            {
                if (!first)
                    out << ",";
                first = false;
                out << "\"" << JsonEscape(it->first) << "\":" << JsonToString(it->second);
            }
            out << "}";
            return out.str();
        }
    }
    return "null";
}

struct ResourceEntry
{
    std::wstring path;
    std::wstring relativePath;
    bool isDirectory = false;
    uint64_t size = 0;
};

struct ServerConfig
{
    std::wstring rootPath;
    std::string serverName = "vampire-files";
    size_t pageSize = 256;
};

class FileIndex
{
public:
    explicit FileIndex(const ServerConfig& config)
        : m_config(config)
    {
    }

    bool Build(std::string& error)
    {
        m_entries.clear();

        if (!PathExists(m_config.rootPath))
        {
            error = "root path does not exist";
            return false;
        }

        ResourceEntry rootEntry;
        rootEntry.path = m_config.rootPath;
        rootEntry.relativePath = L".";
        rootEntry.isDirectory = true;
        m_entries.push_back(rootEntry);

        std::wstring pattern = JoinPath(m_config.rootPath, L"*");
        WalkDirectory(m_config.rootPath, pattern, error);
        if (!error.empty())
            return false;

        std::sort(m_entries.begin(), m_entries.end(),
            [](const ResourceEntry& a, const ResourceEntry& b)
            {
                return _wcsicmp(a.relativePath.c_str(), b.relativePath.c_str()) < 0;
            });

        return true;
    }

    const std::vector<ResourceEntry>& Entries() const { return m_entries; }

private:
    void WalkDirectory(const std::wstring& root, const std::wstring& pattern, std::string& error)
    {
        WIN32_FIND_DATAW data;
        HANDLE handle = FindFirstFileW(pattern.c_str(), &data);
        if (handle == INVALID_HANDLE_VALUE)
            return;

        do
        {
            const wchar_t* name = data.cFileName;
            if (wcscmp(name, L".") == 0 || wcscmp(name, L"..") == 0)
                continue;

            const std::wstring fullPath = JoinPath(root, name);
            const std::wstring canonical = CanonicalizePath(fullPath);
            if (canonical.empty() || !IsWithinRoot(m_config.rootPath, canonical))
                continue;

            ResourceEntry entry;
            entry.path = canonical;
            entry.relativePath = canonical.substr(m_config.rootPath.size());
            if (!entry.relativePath.empty() && (entry.relativePath[0] == L'\\' || entry.relativePath[0] == L'/'))
                entry.relativePath.erase(entry.relativePath.begin());
            entry.isDirectory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            entry.size = (static_cast<uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
            m_entries.push_back(entry);

            if (entry.isDirectory && (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
            {
                WalkDirectory(canonical, JoinPath(canonical, L"*"), error);
                if (!error.empty())
                {
                    FindClose(handle);
                    return;
                }
            }
        }
        while (FindNextFileW(handle, &data) != 0);

        const DWORD lastError = GetLastError();
        FindClose(handle);
        if (lastError != ERROR_NO_MORE_FILES)
        {
            std::ostringstream out;
            out << "directory scan failed with error " << static_cast<unsigned long>(lastError);
            error = out.str();
        }
    }

    const ServerConfig& m_config;
    std::vector<ResourceEntry> m_entries;
};

std::string BuildFileUri(const std::wstring& path)
{
    std::wstring normalized = path;
    std::replace(normalized.begin(), normalized.end(), L'\\', L'/');
    const std::string utf8 = NarrowFromWide(normalized);
    if (utf8.size() >= 2 && std::isalpha(static_cast<unsigned char>(utf8[0])) && utf8[1] == ':')
        return "file:///" + PercentEncodeUtf8(utf8);
    return "file://" + PercentEncodeUtf8(utf8);
}

bool ResolveUriToPath(const ServerConfig& config, const std::string& uri, std::wstring& outPath)
{
    if (uri.find("file://") != 0)
        return false;

    std::string decoded = PercentDecodeUtf8(uri.substr(7));
    if (!decoded.empty() && decoded[0] == '/')
    {
        if (decoded.size() >= 3 && std::isalpha(static_cast<unsigned char>(decoded[1])) && decoded[2] == ':')
            decoded.erase(decoded.begin());
    }

    std::wstring canonical = CanonicalizePath(WideFromUtf8(decoded));
    if (canonical.empty() || !IsWithinRoot(config.rootPath, canonical))
        return false;

    outPath = canonical;
    return true;
}

std::string BuildDirectoryListing(const std::wstring& path)
{
    std::ostringstream out;
    out << "Directory: " << NarrowFromWide(path) << "\n";

    WIN32_FIND_DATAW data;
    HANDLE handle = FindFirstFileW(JoinPath(path, L"*").c_str(), &data);
    if (handle == INVALID_HANDLE_VALUE)
        return out.str();

    do
    {
        const wchar_t* name = data.cFileName;
        if (wcscmp(name, L".") == 0 || wcscmp(name, L"..") == 0)
            continue;

        const bool isDir = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        const uint64_t size = (static_cast<uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
        out << (isDir ? "[dir]  " : "[file] ") << NarrowFromWide(name);
        if (!isDir)
            out << " (" << size << " bytes)";
        out << "\n";
    }
    while (FindNextFileW(handle, &data) != 0);

    FindClose(handle);
    return out.str();
}

bool ReadBinaryFile(const std::wstring& path, std::vector<uint8_t>& outData)
{
    std::ifstream file(path.c_str(), std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return false;

    const std::streamoff end = file.tellg();
    if (end < 0)
        return false;
    file.seekg(0, std::ios::beg);

    outData.resize(static_cast<size_t>(end));
    if (!outData.empty())
        file.read(reinterpret_cast<char*>(&outData[0]), static_cast<std::streamsize>(outData.size()));
    return file.good() || file.eof();
}

bool LooksLikeUtf8Text(const std::vector<uint8_t>& data)
{
    for (size_t i = 0; i < data.size(); ++i)
    {
        if (data[i] == 0)
            return false;
    }
    return true;
}

JsonValue MakeErrorObject(int code, const std::string& message, const JsonValue& data)
{
    JsonValue error = JsonValue::MakeObject();
    error.objectValue["code"] = JsonValue::MakeNumber(static_cast<double>(code));
    error.objectValue["message"] = JsonValue::MakeString(message);
    if (!data.IsNull())
        error.objectValue["data"] = data;
    return error;
}

JsonValue MakeResponseEnvelope(const JsonValue& id, const JsonValue& result)
{
    JsonValue out = JsonValue::MakeObject();
    out.objectValue["jsonrpc"] = JsonValue::MakeString("2.0");
    out.objectValue["id"] = id;
    out.objectValue["result"] = result;
    return out;
}

JsonValue MakeErrorEnvelope(const JsonValue& id, int code, const std::string& message, const JsonValue& data = JsonValue::MakeNull())
{
    JsonValue out = JsonValue::MakeObject();
    out.objectValue["jsonrpc"] = JsonValue::MakeString("2.0");
    out.objectValue["id"] = id;
    out.objectValue["error"] = MakeErrorObject(code, message, data);
    return out;
}

class McpServer
{
public:
    explicit McpServer(const ServerConfig& config)
        : m_config(config)
        , m_index(config)
    {
    }

    int Run()
    {
        std::string buildError;
        if (!m_index.Build(buildError))
        {
            std::cerr << "Failed to index root: " << buildError << std::endl;
            return 1;
        }

        std::string line;
        while (std::getline(std::cin, line))
        {
            if (line.empty())
                continue;

            JsonValue request;
            JsonParser parser(line);
            if (!parser.Parse(request))
            {
                WriteMessage(MakeErrorEnvelope(JsonValue::MakeNull(), -32700,
                                               "Parse error: " + parser.error));
                continue;
            }

            if (!request.IsObject())
            {
                WriteMessage(MakeErrorEnvelope(JsonValue::MakeNull(), -32600, "Invalid Request"));
                continue;
            }

            const JsonValue* methodValue = request.Find("method");
            const JsonValue* idValue = request.Find("id");
            const JsonValue* paramsValue = request.Find("params");
            const JsonValue id = idValue ? *idValue : JsonValue::MakeNull();

            if (methodValue == nullptr || !methodValue->IsString())
            {
                WriteMessage(MakeErrorEnvelope(id, -32600, "Invalid Request"));
                continue;
            }

            JsonValue response;
            const bool shouldReply = HandleRequest(methodValue->stringValue,
                                                   paramsValue ? *paramsValue : JsonValue::MakeNull(),
                                                   id, response);
            if (shouldReply)
                WriteMessage(response);
        }

        return 0;
    }

private:
    bool HandleRequest(const std::string& method, const JsonValue& params, const JsonValue& id, JsonValue& response)
    {
        if (method == "initialize")
        {
            response = HandleInitialize(id);
            return true;
        }

        if (method == "notifications/initialized")
        {
            m_initialized = true;
            return false;
        }

        if (method == "ping")
        {
            JsonValue result = JsonValue::MakeObject();
            response = MakeResponseEnvelope(id, result);
            return true;
        }

        if (method == "resources/list")
        {
            response = HandleResourcesList(id, params);
            return true;
        }

        if (method == "resources/templates/list")
        {
            response = HandleResourceTemplatesList(id);
            return true;
        }

        if (method == "resources/read")
        {
            response = HandleResourcesRead(id, params);
            return true;
        }

        response = MakeErrorEnvelope(id, -32601, "Method not found");
        return true;
    }

    JsonValue HandleInitialize(const JsonValue& id)
    {
        JsonValue result = JsonValue::MakeObject();
        result.objectValue["protocolVersion"] = JsonValue::MakeString(kProtocolVersion);

        JsonValue capabilities = JsonValue::MakeObject();
        capabilities.objectValue["resources"] = JsonValue::MakeObject();
        result.objectValue["capabilities"] = capabilities;

        JsonValue serverInfo = JsonValue::MakeObject();
        serverInfo.objectValue["name"] = JsonValue::MakeString(m_config.serverName);
        serverInfo.objectValue["title"] = JsonValue::MakeString("Vampire File MCP Server");
        serverInfo.objectValue["version"] = JsonValue::MakeString("0.1.0");
        result.objectValue["serverInfo"] = serverInfo;

        std::ostringstream instructions;
        instructions << "Exposes files rooted at " << NarrowFromWide(m_config.rootPath)
                     << ". Read only resources outside this root are rejected.";
        result.objectValue["instructions"] = JsonValue::MakeString(instructions.str());

        return MakeResponseEnvelope(id, result);
    }

    JsonValue HandleResourcesList(const JsonValue& id, const JsonValue& params)
    {
        size_t cursor = 0;
        if (params.IsObject())
        {
            const JsonValue* cursorValue = params.Find("cursor");
            if (cursorValue != nullptr)
            {
                if (cursorValue->IsString())
                    cursor = static_cast<size_t>(std::strtoull(cursorValue->stringValue.c_str(), nullptr, 10));
                else if (cursorValue->IsNumber() && cursorValue->numberValue >= 0.0)
                    cursor = static_cast<size_t>(cursorValue->numberValue);
            }
        }

        JsonValue result = JsonValue::MakeObject();
        JsonValue resources = JsonValue::MakeArray();

        const std::vector<ResourceEntry>& entries = m_index.Entries();
        const size_t end = (std::min)(entries.size(), cursor + m_config.pageSize);
        for (size_t i = cursor; i < end; ++i)
        {
            const ResourceEntry& entry = entries[i];

            JsonValue resource = JsonValue::MakeObject();
            resource.objectValue["uri"] = JsonValue::MakeString(BuildFileUri(entry.path));

            std::wstring displayName = entry.relativePath == L"." ? L"." : entry.relativePath;
            const size_t slash = displayName.find_last_of(L"\\/");
            const std::wstring leaf = (slash == std::wstring::npos) ? displayName : displayName.substr(slash + 1);
            resource.objectValue["name"] = JsonValue::MakeString(NarrowFromWide(leaf));
            resource.objectValue["title"] = JsonValue::MakeString(NarrowFromWide(displayName));
            resource.objectValue["mimeType"] = JsonValue::MakeString(GuessMimeType(entry.path, entry.isDirectory));
            if (!entry.isDirectory)
                resource.objectValue["size"] = JsonValue::MakeNumber(static_cast<double>(entry.size));

            resources.arrayValue.push_back(resource);
        }

        result.objectValue["resources"] = resources;
        if (end < entries.size())
            result.objectValue["nextCursor"] = JsonValue::MakeString(std::to_string(end));

        return MakeResponseEnvelope(id, result);
    }

    JsonValue HandleResourceTemplatesList(const JsonValue& id)
    {
        JsonValue result = JsonValue::MakeObject();
        JsonValue templates = JsonValue::MakeArray();

        JsonValue resourceTemplate = JsonValue::MakeObject();
        resourceTemplate.objectValue["uriTemplate"] =
            JsonValue::MakeString(BuildFileUri(m_config.rootPath) + "/{path}");
        resourceTemplate.objectValue["name"] = JsonValue::MakeString("project-files");
        resourceTemplate.objectValue["title"] = JsonValue::MakeString("Project Files");
        resourceTemplate.objectValue["description"] =
            JsonValue::MakeString("Read files rooted under the configured workspace folder.");
        resourceTemplate.objectValue["mimeType"] = JsonValue::MakeString("application/octet-stream");
        templates.arrayValue.push_back(resourceTemplate);

        result.objectValue["resourceTemplates"] = templates;
        return MakeResponseEnvelope(id, result);
    }

    JsonValue HandleResourcesRead(const JsonValue& id, const JsonValue& params)
    {
        if (!params.IsObject())
            return MakeErrorEnvelope(id, -32602, "Invalid params");

        const JsonValue* uriValue = params.Find("uri");
        if (uriValue == nullptr || !uriValue->IsString())
            return MakeErrorEnvelope(id, -32602, "Invalid params: missing uri");

        std::wstring path;
        if (!ResolveUriToPath(m_config, uriValue->stringValue, path) || !PathExists(path))
        {
            JsonValue data = JsonValue::MakeObject();
            data.objectValue["uri"] = JsonValue::MakeString(uriValue->stringValue);
            return MakeErrorEnvelope(id, kResourceNotFound, "Resource not found", data);
        }

        JsonValue result = JsonValue::MakeObject();
        JsonValue contents = JsonValue::MakeArray();
        JsonValue content = JsonValue::MakeObject();

        const bool isDirectory = IsDirectoryPath(path);
        const std::string mimeType = GuessMimeType(path, isDirectory);
        content.objectValue["uri"] = JsonValue::MakeString(uriValue->stringValue);
        content.objectValue["mimeType"] = JsonValue::MakeString(mimeType);

        if (isDirectory)
        {
            content.objectValue["text"] = JsonValue::MakeString(BuildDirectoryListing(path));
        }
        else
        {
            std::vector<uint8_t> bytes;
            if (!ReadBinaryFile(path, bytes))
                return MakeErrorEnvelope(id, -32603, "Failed to read file");

            if (IsLikelyTextMime(mimeType) || LooksLikeUtf8Text(bytes))
                content.objectValue["text"] = JsonValue::MakeString(std::string(bytes.begin(), bytes.end()));
            else
                content.objectValue["blob"] = JsonValue::MakeString(Base64Encode(bytes));
        }

        contents.arrayValue.push_back(content);
        result.objectValue["contents"] = contents;
        return MakeResponseEnvelope(id, result);
    }

    void WriteMessage(const JsonValue& message)
    {
        std::cout << JsonToString(message) << "\n";
        std::cout.flush();
    }

    ServerConfig m_config;
    FileIndex m_index;
    bool m_initialized = false;
};

bool ParseArguments(int argc, wchar_t** argv, ServerConfig& config, std::string& error)
{
    std::wstring rootPath;
    wchar_t currentDir[MAX_PATH];
    if (GetCurrentDirectoryW(MAX_PATH, currentDir) == 0)
    {
        error = "failed to query current directory";
        return false;
    }
    rootPath = currentDir;

    for (int i = 1; i < argc; ++i)
    {
        const std::wstring arg = argv[i];
        if (arg == L"--root")
        {
            if (i + 1 >= argc)
            {
                error = "--root requires a path";
                return false;
            }
            rootPath = argv[++i];
        }
        else if (arg == L"--page-size")
        {
            if (i + 1 >= argc)
            {
                error = "--page-size requires a value";
                return false;
            }
            config.pageSize = static_cast<size_t>(_wtoi(argv[++i]));
            if (config.pageSize == 0)
                config.pageSize = 256;
        }
        else if (arg == L"--name")
        {
            if (i + 1 >= argc)
            {
                error = "--name requires a value";
                return false;
            }
            config.serverName = NarrowFromWide(argv[++i]);
        }
        else if (arg == L"--help" || arg == L"-h")
        {
            std::wcerr << L"Usage: vampire_mcp_server.exe [--root <path>] [--page-size <n>] [--name <value>]\n";
            std::exit(0);
        }
        else
        {
            error = "unknown argument";
            return false;
        }
    }

    config.rootPath = CanonicalizePath(rootPath);
    if (config.rootPath.empty())
    {
        error = "failed to canonicalize root path";
        return false;
    }

    return true;
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
    ServerConfig config;
    std::string error;
    if (!ParseArguments(argc, argv, config, error))
    {
        std::cerr << "Argument error: " << error << std::endl;
        return 1;
    }

    McpServer server(config);
    return server.Run();
}

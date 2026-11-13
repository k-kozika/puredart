#include "purehermes/bytecode.h"

#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <stdexcept>
namespace purehermes::detail {
struct Op {
    const char* name;
    uint8_t string_mask;
    uint8_t count;
    int8_t widths[6];
};
struct Field {
    const char* name;
    uint8_t bits, full_width;
};
struct Format {
    const Op* ops;
    size_t op_count;
    const char* const* header;
    size_t header_count;
    const Field* fields;
    size_t field_count;
    size_t function_size;
};
}  // namespace purehermes::detail
#include "formats.inc"
namespace purehermes {
namespace {
struct Reader {
    const std::vector<uint8_t>& b;
    void range(size_t p, size_t n) const {
        if (p > b.size() || n > b.size() - p) throw std::runtime_error("truncated HBC range");
    }
    uint64_t u(size_t p, size_t n) const {
        range(p, n);
        uint64_t v = 0;
        for (size_t i = 0; i < n; ++i) v |= uint64_t(b[p + i]) << (8 * i);
        return v;
    }
};
uint32_t get(const std::map<std::string, uint32_t>& m, const char* s) {
    auto i = m.find(s);
    return i == m.end() ? 0 : i->second;
}
void utf8(std::string& s, uint32_t c) {
    if (c < 0x80)
        s += char(c);
    else if (c < 0x800) {
        s += char(0xc0 | (c >> 6));
        s += char(0x80 | (c & 63));
    } else if (c < 0x10000) {
        s += char(0xe0 | (c >> 12));
        s += char(0x80 | ((c >> 6) & 63));
        s += char(0x80 | (c & 63));
    } else {
        s += char(0xf0 | (c >> 18));
        s += char(0x80 | ((c >> 12) & 63));
        s += char(0x80 | ((c >> 6) & 63));
        s += char(0x80 | (c & 63));
    }
}
}  // namespace
Bundle Bundle::read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open HBC file");
    std::vector<uint8_t> b{std::istreambuf_iterator<char>(f), {}};
    return read(b.data(), b.size());
}
Bundle Bundle::read(const uint8_t* bytes, size_t size) {
    if (!bytes || size < 128) throw std::runtime_error("HBC header is truncated");
    Bundle out;
    out.bytes_.assign(bytes, bytes + size);
    Reader r{out.bytes_};
    if (r.u(0, 8) != 0x1f1903c103bc1fc6ULL) throw std::runtime_error("invalid HBC magic");
    out.version_ = uint32_t(r.u(8, 4));
    const auto fmt = detail::format(out.version_);
    std::map<std::string, uint32_t> h;
    for (size_t i = 0; i < fmt.header_count; ++i) h[fmt.header[i]] = uint32_t(r.u(32 + i * 4, 4));
    if (get(h, "fileLength") != size) throw std::runtime_error("HBC length mismatch");
    const size_t nf = get(h, "functionCount"), ns = get(h, "stringCount");
    if (nf == 0 || nf > (size - 128) / fmt.function_size)
        throw std::runtime_error("invalid function table");
    size_t p = 128;
    auto take = [&](size_t count, size_t width) {
        if (count > size / width) throw std::runtime_error("HBC count overflow");
        size_t start = p;
        r.range(p, count * width);
        p += count * width;
        p = (p + 3) & ~size_t(3);
        r.range(p, 0);
        return start;
    };
    const size_t functions = take(nf, fmt.function_size);
    const size_t kinds = take(get(h, "stringKindCount"), 4);
    take(get(h, "identifierCount"), 4);
    const size_t strings = take(ns, 4), overflow = take(get(h, "overflowStringCount"), 8);
    const size_t storage = take(get(h, "stringStorageSize"), 1);
    size_t kind_index = 0;
    for (size_t k = 0; k < get(h, "stringKindCount"); ++k) {
        auto entry = uint32_t(r.u(kinds + k * 4, 4));
        size_t count = entry & 0x7fffffff;
        if (count == 0 || count > ns - kind_index)
            throw std::runtime_error("invalid string-kind run");
        out.identifiers_.insert(out.identifiers_.end(), count, uint8_t(entry >> 31));
        kind_index += count;
    }
    if (kind_index != ns) throw std::runtime_error("string-kind coverage mismatch");
    for (size_t i = 0; i < ns; ++i) {
        uint32_t e = uint32_t(r.u(strings + i * 4, 4)), off = (e >> 1) & 0x7fffff, len = e >> 24;
        if (len == 255) {
            if (off >= get(h, "overflowStringCount"))
                throw std::runtime_error("invalid overflow string index");
            len = uint32_t(r.u(overflow + off * 8 + 4, 4));
            off = uint32_t(r.u(overflow + off * 8, 4));
        }
        const size_t width = (e & 1) ? 2 : 1;
        if (off > get(h, "stringStorageSize") ||
            uint64_t(len) * width > get(h, "stringStorageSize") - off)
            throw std::runtime_error("invalid string storage range");
        std::string value;
        for (size_t j = 0; j < len; ++j) {
            uint32_t c = uint32_t(r.u(storage + off + j * width, width));
            if (width == 2 && c >= 0xd800 && c <= 0xdbff && j + 1 < len) {
                uint32_t lo = uint32_t(r.u(storage + off + (j + 1) * 2, 2));
                if (lo >= 0xdc00 && lo <= 0xdfff) {
                    c = 0x10000 + ((c - 0xd800) << 10) + (lo - 0xdc00);
                    ++j;
                }
            }
            if (width == 1)
                value += char(c);
            else
                utf8(value, c);
        }
        out.strings_.push_back(std::move(value));
    }
    // Validate the remaining fixed tables, even when only function metadata is requested.
    size_t arrays = 0, keys = 0, literal_values = 0, shapes = 0;
    if (out.version_ == 96) {
        arrays = take(get(h, "arrayBufferSize"), 1);
        keys = take(get(h, "objKeyBufferSize"), 1);
        literal_values = take(get(h, "objValueBufferSize"), 1);
    } else {
        arrays = literal_values = take(get(h, "literalValueBufferSize"), 1);
        keys = take(get(h, "objKeyBufferSize"), 1);
        shapes = take(get(h, "objShapeTableCount"), 8);
    }
    auto literals = [&](size_t base, size_t length, uint32_t offset, uint32_t count,
                        uint32_t function_id, uint32_t instruction_offset) {
        size_t pos = offset;
        auto read = [&](size_t n) {
            if (pos > length || n > length - pos)
                throw std::runtime_error("literal buffer out of bounds");
            auto v = r.u(base + pos, n);
            pos += n;
            return v;
        };
        while (count) {
            uint8_t tag = uint8_t(read(1));
            uint32_t run = tag & 15;
            if (tag & 128) run = (run << 8) | uint32_t(read(1));
            if (!run) throw std::runtime_error("empty literal run");
            run = std::min(run, count);
            count -= run;
            uint8_t type = (tag >> 4) & 7;
            size_t width = type == 3                         ? 8
                           : type == 4 || type == 7          ? 4
                           : type == 5                       ? 2
                           : type == 6 && out.version_ <= 97 ? 1
                                                             : 0;
            if (type == 4 || type == 5 || (type == 6 && out.version_ <= 97)) {
                for (uint32_t k = 0; k < run; ++k) {
                    auto id = read(width);
                    if (id >= ns) throw std::runtime_error("invalid literal string ID");
                    out.uses_.push_back({uint32_t(id), function_id, instruction_offset});
                }
            } else {
                size_t n = size_t(run) * width;
                if (pos > length || n > length - pos)
                    throw std::runtime_error("literal payload out of bounds");
                pos += n;
            }
        }
    };
    take(get(h, "bigIntCount"), 8);
    take(get(h, "bigIntStorageSize"), 1);
    take(get(h, "regExpCount"), 8);
    take(get(h, "regExpStorageSize"), 1);
    take(get(h, "cjsModuleCount"), 8);
    take(get(h, "functionSourceCount"), 8);
    const size_t bytecode_start = p;
    for (size_t i = 0; i < nf; ++i) {
        const size_t f = functions + i * fmt.function_size;
        std::map<std::string, uint32_t> values;
        size_t bit = 0;
        for (size_t j = 0; j < fmt.field_count; ++j) {
            const auto field = fmt.fields[j];
            uint64_t val = 0;
            for (size_t k = 0; k < field.bits; ++k)
                val |= ((r.u(f + (bit + k) / 8, 1) >> ((bit + k) % 8)) & 1) << k;
            values[field.name] = uint32_t(val);
            bit += field.bits;
        }
        uint8_t flags = uint8_t(r.u(f + fmt.function_size - 1, 1));
        if (flags & 32) {
            size_t large =
                out.version_ == 96 ? (get(values, "infoOffset") << 16) | get(values, "offset")
                : out.version_ == 97
                    ? (get(values, "functionName") << 16) | get(values, "offset")
                    : (get(values, "offset") & 0xffffff) | (get(values, "functionName") << 24);
            for (size_t j = 0; j < fmt.field_count; ++j) {
                const auto field = fmt.fields[j];
                values[field.name] = uint32_t(r.u(large, field.full_width));
                large += field.full_width;
            }
            flags = uint8_t(r.u(large, 1));
        }
        Function fun;
        fun.kind = flags >> 6;
        fun.id = uint32_t(i);
        fun.offset = get(values, "offset");
        fun.size = get(values, "bytecodeSizeInBytes");
        fun.parameters = get(values, "paramCount");
        fun.registers = get(values, "frameSize");
        const uint32_t name = get(values, "functionName");
        if (name >= ns) throw std::runtime_error("invalid function name");
        fun.name = out.strings_[name];
        r.range(fun.offset, fun.size);
        if (fun.offset < bytecode_start || size < 20 || fun.offset > size - 20 ||
            fun.size > size - 20 - fun.offset)
            throw std::runtime_error("function overlaps HBC tables/footer");
        size_t ip = 0;
        while (ip < fun.size) {
            Instruction in;
            in.offset = uint32_t(ip);
            in.opcode = uint8_t(r.u(fun.offset + ip++, 1));
            if (in.opcode >= fmt.op_count) throw std::runtime_error("invalid opcode");
            const auto& def = fmt.ops[in.opcode];
            in.name = def.name;
            for (size_t j = 0; j < def.count; ++j) {
                int w = def.widths[j];
                size_t n = size_t(w < 0 ? -w : w);
                if (n > fun.size - ip) throw std::runtime_error("truncated instruction");
                uint64_t v = r.u(fun.offset + ip, n);
                ip += n;
                if (w < 0 && (v & (uint64_t(1) << (n * 8 - 1)))) v |= (~uint64_t(0)) << (n * 8);
                int64_t operand;
                std::memcpy(&operand, &v, 8);
                in.operands.push_back(operand);
            }
            for (size_t operand = 0; operand < def.count; ++operand)
                if (def.string_mask & (1u << operand)) {
                    auto id = uint32_t(in.operands.at(operand));
                    if (id >= ns) throw std::runtime_error("invalid instruction string ID");
                    out.uses_.push_back({id, fun.id, in.offset});
                }
            if (in.name == "NewArrayWithBuffer" || in.name == "NewArrayWithBufferLong")
                literals(arrays,
                         get(h, out.version_ == 96 ? "arrayBufferSize" : "literalValueBufferSize"),
                         uint32_t(in.operands.at(3)), uint32_t(in.operands.at(2)), fun.id,
                         in.offset);
            if (in.name == "NewObjectWithBuffer" || in.name == "NewObjectWithBufferLong" ||
                in.name == "NewObjectWithBufferAndParent") {
                uint32_t count, ko, vo;
                if (out.version_ == 96) {
                    count = uint32_t(in.operands.at(2));
                    ko = uint32_t(in.operands.at(3));
                    vo = uint32_t(in.operands.at(4));
                } else {
                    size_t idx = in.name == "NewObjectWithBufferAndParent" ? 2 : 1;
                    auto shape = uint32_t(in.operands.at(idx));
                    if (shape >= get(h, "objShapeTableCount"))
                        throw std::runtime_error("invalid shape ID");
                    ko = uint32_t(r.u(shapes + shape * 8, 4));
                    count = uint32_t(r.u(shapes + shape * 8 + 4, 4));
                    vo = uint32_t(in.operands.at(idx + 1));
                }
                literals(keys, get(h, "objKeyBufferSize"), ko, count, fun.id, in.offset);
                literals(
                    literal_values,
                    get(h, out.version_ == 96 ? "objValueBufferSize" : "literalValueBufferSize"),
                    vo, count, fun.id, in.offset);
            }
            if (in.name == "Ret") fun.returns.push_back(in.offset);
            fun.instructions.push_back(std::move(in));
        }
        out.functions_.push_back(std::move(fun));
    }
    return out;
}
const Function& Bundle::function(uint32_t id) const {
    if (id >= functions_.size()) throw std::out_of_range("function ID");
    return functions_[id];
}
const Function& Bundle::function(const std::string& name) const {
    const Function* found = nullptr;
    for (const auto& f : functions_)
        if (f.name == name) {
            if (found) throw std::runtime_error("ambiguous function name; use ID");
            found = &f;
        }
    if (!found) throw std::runtime_error("function not found");
    return *found;
}
std::vector<uint32_t> supported_versions() { return {96, 97, 98, 99}; }
}  // namespace purehermes

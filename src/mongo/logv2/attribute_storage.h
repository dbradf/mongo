/**
 *    Copyright (C) 2019-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#pragma once

#include "mongo/base/string_data.h"
#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/logv2/constants.h"
#include "mongo/stdx/variant.h"

#include <functional>

namespace mongo {
namespace logv2 {

class TypeErasedAttributeStorage;

// Custom type, storage for how to call its formatters
struct CustomAttributeValue {
    std::function<void(BSONObjBuilder&)> BSONSerialize;
    std::function<BSONArray()> toBSONArray;
    std::function<void(BSONObjBuilder&, StringData)> BSONAppend;

    // Have both serialize and toString available, using toString() with a serialize interface is
    // inefficient.
    std::function<void(fmt::memory_buffer&)> stringSerialize;
    std::function<std::string()> toString;
};

template <typename T>
auto seqLog(const T& container);

template <typename It>
auto seqLog(It begin, It end);

template <typename T>
auto mapLog(const T& container);

template <typename It>
auto mapLog(It begin, It end);

namespace detail {
namespace {

// Helper traits to figure out capabilities on custom types
template <class T>
struct IsOptional : std::false_type {};

template <class T>
struct IsOptional<boost::optional<T>> : std::true_type {};

template <class T, typename = void>
struct IsContainer : std::false_type {};

template <class T, typename = void>
struct HasMappedType : std::false_type {};

template <typename T>
struct HasMappedType<T, std::void_t<typename T::mapped_type>> : std::true_type {};

// Trait to detect container, common interface for both std::array and std::forward_list
template <typename T>
struct IsContainer<T,
                   std::void_t<typename T::value_type,
                               typename T::size_type,
                               typename T::iterator,
                               typename T::const_iterator,
                               decltype(std::declval<T>().empty()),
                               decltype(std::declval<T>().begin()),
                               decltype(std::declval<T>().end())>> : std::true_type {};

template <class T, class = void>
struct HasToBSON : std::false_type {};

template <class T>
struct HasToBSON<T, std::void_t<decltype(std::declval<T>().toBSON())>> : std::true_type {};

template <class T, class = void>
struct HasToBSONArray : std::false_type {};

template <class T>
struct HasToBSONArray<T, std::void_t<decltype(std::declval<T>().toBSONArray())>> : std::true_type {
};

template <class T, class = void>
struct HasBSONSerialize : std::false_type {};

template <class T>
struct HasBSONSerialize<
    T,
    std::void_t<decltype(std::declval<T>().serialize(std::declval<BSONObjBuilder*>()))>>
    : std::true_type {};

template <class T, class = void>
struct HasBSONBuilderAppend : std::false_type {};

template <class T>
struct HasBSONBuilderAppend<T,
                            std::void_t<decltype(std::declval<BSONObjBuilder>().append(
                                std::declval<StringData>(), std::declval<T>()))>> : std::true_type {
};

template <class T, class = void>
struct HasStringSerialize : std::false_type {};

template <class T>
struct HasStringSerialize<
    T,
    std::void_t<decltype(std::declval<T>().serialize(std::declval<fmt::memory_buffer&>()))>>
    : std::true_type {};

template <class T, class = void>
struct HasToString : std::false_type {};

template <class T>
struct HasToString<T, std::void_t<decltype(std::declval<T>().toString())>> : std::true_type {};

template <class T, class = void>
struct HasNonMemberToString : std::false_type {};

template <class T>
struct HasNonMemberToString<T, std::void_t<decltype(toString(std::declval<T>()))>>
    : std::true_type {};

template <class T, class = void>
struct HasNonMemberToBSON : std::false_type {};

template <class T>
struct HasNonMemberToBSON<T, std::void_t<decltype(toBSON(std::declval<T>()))>> : std::true_type {};

}  // namespace

// Mapping functions on how to map a logged value to how it is stored in variant (reused by
// container support)
inline bool mapValue(bool value) {
    return value;
}
inline int mapValue(int value) {
    return value;
}
inline unsigned int mapValue(unsigned int value) {
    return value;
}
inline long long mapValue(long value) {
    return value;
}
inline unsigned long long mapValue(unsigned long value) {
    return value;
}
inline long long mapValue(long long value) {
    return value;
}
inline unsigned long long mapValue(unsigned long long value) {
    return value;
}
inline double mapValue(float value) {
    return value;
}
inline double mapValue(double value) {
    return value;
}
inline StringData mapValue(StringData value) {
    return value;
}
inline StringData mapValue(std::string const& value) {
    return value;
}
inline StringData mapValue(char* value) {
    return value;
}
inline StringData mapValue(const char* value) {
    return value;
}
inline const BSONObj* mapValue(BSONObj const& value) {
    return &value;
}
inline const BSONArray* mapValue(BSONArray const& value) {
    return &value;
}
inline CustomAttributeValue mapValue(BSONElement const& val) {
    CustomAttributeValue custom;
    custom.BSONSerialize = [&val](BSONObjBuilder& builder) { builder.appendElements(val.wrap()); };
    custom.toString = [&val]() { return val.toString(); };
    return custom;
}
inline CustomAttributeValue mapValue(boost::none_t val) {
    CustomAttributeValue custom;
    // Use BSONAppend instead of toBSON because we just want the null value and not a whole
    // object with a field name
    custom.BSONAppend = [](BSONObjBuilder& builder, StringData fieldName) {
        builder.appendNull(fieldName);
    };
    custom.toString = [&val]() { return constants::kNullOptionalString.toString(); };
    return custom;
}

template <typename T, std::enable_if_t<std::is_enum_v<T>, int> = 0>
auto mapValue(T val) {
    if constexpr (HasNonMemberToString<T>::value) {
        CustomAttributeValue custom;
        custom.toString = [val]() { return toString(val); };
        return custom;
    } else {
        return mapValue(static_cast<std::underlying_type_t<T>>(val));
    }
}

template <typename T, std::enable_if_t<IsContainer<T>::value && !HasMappedType<T>::value, int> = 0>
CustomAttributeValue mapValue(const T& val) {
    CustomAttributeValue custom;
    custom.toBSONArray = [&val]() { return seqLog(val).toBSONArray(); };
    custom.stringSerialize = [&val](fmt::memory_buffer& buffer) { seqLog(val).serialize(buffer); };
    return custom;
}

template <typename T, std::enable_if_t<IsContainer<T>::value && HasMappedType<T>::value, int> = 0>
CustomAttributeValue mapValue(const T& val) {
    CustomAttributeValue custom;
    custom.BSONSerialize = [&val](BSONObjBuilder& builder) { mapLog(val).serialize(&builder); };
    custom.stringSerialize = [&val](fmt::memory_buffer& buffer) { mapLog(val).serialize(buffer); };
    return custom;
}

template <typename T,
          std::enable_if_t<!std::is_integral_v<T> && !std::is_floating_point_v<T> &&
                               !std::is_enum_v<T> && !IsContainer<T>::value,
                           int> = 0>
CustomAttributeValue mapValue(const T& val) {
    static_assert(HasToString<T>::value || HasStringSerialize<T>::value ||
                      HasNonMemberToString<T>::value,
                  "custom type needs toString() or serialize(fmt::memory_buffer&) implementation");

    CustomAttributeValue custom;
    if constexpr (HasBSONBuilderAppend<T>::value) {
        custom.BSONAppend = [&val](BSONObjBuilder& builder, StringData fieldName) {
            builder.append(fieldName, val);
        };
    }
    if constexpr (HasBSONSerialize<T>::value) {
        custom.BSONSerialize = [&val](BSONObjBuilder& builder) { val.serialize(&builder); };
    } else if constexpr (HasToBSON<T>::value) {
        custom.BSONSerialize = [&val](BSONObjBuilder& builder) {
            builder.appendElements(val.toBSON());
        };
    } else if constexpr (HasToBSONArray<T>::value) {
        custom.toBSONArray = [&val]() { return val.toBSONArray(); };
    } else if constexpr (HasNonMemberToBSON<T>::value) {
        custom.BSONSerialize = [&val](BSONObjBuilder& builder) {
            builder.appendElements(toBSON(val));
        };
    }
    if constexpr (HasStringSerialize<T>::value) {
        custom.stringSerialize = [&val](fmt::memory_buffer& buffer) { val.serialize(buffer); };
    } else if constexpr (HasToString<T>::value) {
        custom.toString = [&val]() { return val.toString(); };
    } else if constexpr (HasNonMemberToString<T>::value) {
        custom.toString = [&val]() { return toString(val); };
    }

    return custom;
}

template <typename It>
class SequenceContainerLogger {
public:
    SequenceContainerLogger(It begin, It end) : _begin(begin), _end(end) {}

    // JSON Format: [elem1, elem2, ..., elemN]
    BSONArray toBSONArray() const {
        BSONArrayBuilder builder;
        for (auto it = _begin; it != _end; ++it) {
            const auto& item = *it;
            auto append = [&builder](auto&& val) {
                if constexpr (std::is_same_v<decltype(val), CustomAttributeValue&&>) {
                    if (val.BSONAppend) {
                        BSONObjBuilder objBuilder;
                        val.BSONAppend(objBuilder, ""_sd);
                        builder.append(objBuilder.done().getField(""_sd));
                    } else if (val.BSONSerialize) {
                        BSONObjBuilder objBuilder;
                        val.BSONSerialize(objBuilder);
                        builder.append(objBuilder.done());
                    } else if (val.toBSONArray) {
                        builder.append(val.toBSONArray());
                    } else if (val.stringSerialize) {
                        fmt::memory_buffer buffer;
                        val.stringSerialize(buffer);
                        builder.append(fmt::to_string(buffer));
                    } else {
                        builder.append(val.toString());
                    }
                } else {
                    builder.append(val);
                }
            };

            using item_t = std::decay_t<decltype(item)>;
            if constexpr (IsOptional<item_t>::value) {
                if (item) {
                    append(mapValue(*item));
                } else {
                    append(mapValue(boost::none));
                }
            } else {
                append(mapValue(item));
            }
        }
        return builder.arr();
    }

    // Text Format: (elem1, elem2, ..., elemN)
    void serialize(fmt::memory_buffer& buffer) const {
        StringData separator = ""_sd;
        buffer.push_back('(');
        for (auto it = _begin; it != _end; ++it) {
            const auto& item = *it;
            buffer.append(separator.begin(), separator.end());

            auto append = [&buffer](auto&& val) {
                if constexpr (std::is_same_v<decltype(val), CustomAttributeValue&&>) {
                    if (val.stringSerialize) {
                        val.stringSerialize(buffer);
                    } else {
                        fmt::format_to(buffer, "{}", val.toString());
                    }
                } else {
                    fmt::format_to(buffer, "{}", val);
                }
            };

            using item_t = std::decay_t<decltype(item)>;
            if constexpr (IsOptional<item_t>::value) {
                if (item) {
                    append(mapValue(*item));
                } else {
                    append(mapValue(boost::none));
                }
            } else {
                append(mapValue(item));
            }

            separator = ", "_sd;
        }
        buffer.push_back(')');
    }

private:
    It _begin;
    It _end;
};

template <typename It>
class AssociativeContainerLogger {
public:
    static_assert(std::is_same_v<decltype(mapValue(std::declval<It>()->first)), StringData>,
                  "key in associative container needs to be a string");

    AssociativeContainerLogger(It begin, It end) : _begin(begin), _end(end) {}

    // JSON Format: {"elem1": val1, "elem2": val2, ..., "elemN": valN}
    void serialize(BSONObjBuilder* builder) const {
        for (auto it = _begin; it != _end; ++it) {
            const auto& item = *it;
            auto append = [builder](StringData key, auto&& val) {
                if constexpr (std::is_same_v<decltype(val), CustomAttributeValue&&>) {
                    if (val.BSONAppend) {
                        val.BSONAppend(*builder, key);
                    } else if (val.BSONSerialize) {
                        BSONObjBuilder subBuilder = builder->subobjStart(key);
                        val.BSONSerialize(subBuilder);
                        subBuilder.done();
                    } else if (val.toBSONArray) {
                        builder->append(key, val.toBSONArray());
                    } else if (val.stringSerialize) {
                        fmt::memory_buffer buffer;
                        val.stringSerialize(buffer);
                        builder->append(key, fmt::to_string(buffer));
                    } else {
                        builder->append(key, val.toString());
                    }
                } else {
                    builder->append(key, val);
                }
            };
            auto key = mapValue(item.first);
            using value_t = std::decay_t<decltype(item.second)>;
            if constexpr (IsOptional<value_t>::value) {
                if (item.second) {
                    append(key, mapValue(*item.second));
                } else {
                    append(key, mapValue(boost::none));
                }
            } else {
                append(key, mapValue(item.second));
            }
        }
    }

    // Text Format: (elem1: val1, elem2: val2, ..., elemN: valN)
    void serialize(fmt::memory_buffer& buffer) const {
        StringData separator = ""_sd;
        buffer.push_back('(');
        for (auto it = _begin; it != _end; ++it) {
            const auto& item = *it;
            buffer.append(separator.begin(), separator.end());

            auto append = [&buffer](StringData key, auto&& val) {
                if constexpr (std::is_same_v<decltype(val), CustomAttributeValue&&>) {
                    if (val.stringSerialize) {
                        fmt::format_to(buffer, "{}: ", key);
                        val.stringSerialize(buffer);
                    } else {
                        fmt::format_to(buffer, "{}: {}", key, val.toString());
                    }
                } else {
                    fmt::format_to(buffer, "{}: {}", key, val);
                }
            };

            auto key = mapValue(item.first);
            using value_t = std::decay_t<decltype(item.second)>;
            if constexpr (IsOptional<value_t>::value) {
                if (item.second) {
                    append(key, mapValue(*item.second));
                } else {
                    append(key, mapValue(boost::none));
                }
            } else {
                append(key, mapValue(item.second));
            }

            separator = ", "_sd;
        }
        buffer.push_back(')');
    }

private:
    It _begin;
    It _end;
};

// Named attribute, storage for a name-value attribute.
class NamedAttribute {
public:
    NamedAttribute() = default;
    NamedAttribute(StringData n, long double val) = delete;

    template <typename T>
    NamedAttribute(StringData n, const boost::optional<T>& val)
        : NamedAttribute(val ? NamedAttribute(n, *val) : NamedAttribute()) {
        if (!val) {
            name = n;
            value = mapValue(boost::none);
        }
    }

    template <typename T>
    NamedAttribute(StringData n, const T& val) : name(n), value(mapValue(val)) {}

    StringData name;
    stdx::variant<int,
                  unsigned int,
                  long long,
                  unsigned long long,
                  bool,
                  double,
                  StringData,
                  const BSONObj*,
                  const BSONArray*,
                  CustomAttributeValue>
        value;
};

// Attribute Storage stores an array of Named Attributes.
template <typename... Args>
class AttributeStorage {
public:
    AttributeStorage(const Args&... args)
        : _data{detail::NamedAttribute(StringData(args.name.data(), args.name.size()),
                                       args.value)...} {}

private:
    static const size_t kNumArgs = sizeof...(Args);

    // Arrays need a size of at least 1, add dummy element if needed (kNumArgs above is still 0)
    NamedAttribute _data[kNumArgs ? kNumArgs : 1];

    // This class is meant to be wrapped by TypeErasedAttributeStorage below that provides public
    // accessors. Let it access all our internals.
    friend class mongo::logv2::TypeErasedAttributeStorage;
};

template <typename... Args>
AttributeStorage<Args...> makeAttributeStorage(const Args&... args) {
    return {args...};
}

}  // namespace detail

// Wrapper around internal pointer of AttributeStorage so it does not need any template parameters
class TypeErasedAttributeStorage {
public:
    TypeErasedAttributeStorage() : _size(0) {}

    template <typename... Args>
    TypeErasedAttributeStorage(const detail::AttributeStorage<Args...>& store) {
        _data = store._data;
        _size = store.kNumArgs;
    }

    bool empty() const {
        return _size == 0;
    }

    std::size_t size() const {
        return _size;
    }

    // Applies a function to every stored named attribute in order they are captured
    template <typename Func>
    void apply(Func&& f) const {
        std::for_each(_data, _data + _size, [&f](const detail::NamedAttribute& attr) {
            StringData name = attr.name;
            stdx::visit([name, &f](auto&& val) { f(name, val); }, attr.value);
        });
    }

private:
    const detail::NamedAttribute* _data;
    size_t _size;
};

// Helpers for logging containers, optional to use. Allowes logging of ranges.
template <typename T>
auto seqLog(const T& container) {
    using std::begin;
    using std::end;
    return detail::SequenceContainerLogger(begin(container), end(container));
}

template <typename It>
auto seqLog(It begin, It end) {
    return detail::SequenceContainerLogger(begin, end);
}

template <typename T>
auto mapLog(const T& container) {
    using std::begin;
    using std::end;
    return detail::AssociativeContainerLogger(begin(container), end(container));
}

template <typename It>
auto mapLog(It begin, It end) {
    return detail::AssociativeContainerLogger(begin, end);
}

}  // namespace logv2
}  // namespace mongo

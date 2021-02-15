#ifndef XKC_H
#define XKC_H

#include "bits.h"
#include "buffer.h"
#include "types.h"

#include <algorithm>
#include <limits>
#include <memory>

namespace XLib
{
    template <typename alphabet_T = byte_t>
    class XKC
    {
      public:
        static auto constexpr decide_alphabet_type()
        {
            if constexpr (sizeof(alphabet_T) == 1)
            {
                return type_wrapper<int16_t>;
            }
            else if constexpr (sizeof(alphabet_T) == 2)
            {
                return type_wrapper<int32_t>;
            }
            else if constexpr (sizeof(alphabet_T) == 4)
            {
                return type_wrapper<int64_t>;
            }
            else
            {
                static_assert("Not supported");
            }
        }

        using value_t = typename decltype(decide_alphabet_type())::type;

        using bit_path_t = std::bitset<
          std::numeric_limits<alphabet_T>::max() + 1>;

        struct Occurrence
        {
            alphabet_T letter_value;
            /* let's limit the occurences to 256 times */
            byte_t count = 0;
        };

        struct Letter
        {
            alphabet_T value;
            size_t freq = 0;
        };

        struct PathInfo
        {
            bit_path_t bit_path = 0;
            size_t depth        = 0;
        };

        struct PathInfoResult : PathInfo
        {
            alphabet_T letter_value;
        };

        struct BinaryTree
        {
            struct Node;
            using shared_node = std::shared_ptr<Node>;

            struct Node
            {
                enum Value : value_t
                {
                    INVALID = -1
                };

                auto height() -> size_t;
                auto depth() -> size_t;
                auto count_nodes() -> size_t;

                shared_node root   = nullptr;
                shared_node parent = nullptr;
                value_t value      = INVALID;
                shared_node left   = nullptr;
                shared_node right  = nullptr;
            };

            BinaryTree();

            void insert(shared_node parent, alphabet_T value);
            void insert(alphabet_T value);

            bool path_info(PathInfo& pathInfo,
                           shared_node parent,
                           alphabet_T value);

            bool path_info(PathInfo& pathInfo, alphabet_T value);

            void find_value(PathInfoResult& pathInfo);

            std::string dot_format(shared_node parent);
            std::string dot_format();

            shared_node root;
        };

        using alphabet_t    = std::vector<Letter>;
        using occurrences_t = std::vector<Occurrence>;

      public:
        static bytes_t encode(data_t data, size_t size);
        static bytes_t encode(bytes_t bytes);

        static bytes_t decode(data_t data, size_t size);
        static bytes_t decode(bytes_t bytes);
    };
};

template <typename alphabet_T>
size_t XLib::XKC<alphabet_T>::BinaryTree::Node::count_nodes()
{
    size_t result = 0;

    if (left)
    {
        result += left->count_nodes();
        result++;
    }

    if (right)
    {
        result += right->count_nodes();
        result++;
    }

    return result;
}

template <typename alphabet_T>
size_t XLib::XKC<alphabet_T>::BinaryTree::Node::depth()
{
    size_t result = 0;
    auto node     = parent;

    while (node)
    {
        result++;
        node = node->parent;
    }

    return result;
}

template <typename alphabet_T>
size_t XLib::XKC<alphabet_T>::BinaryTree::Node::height()
{
    size_t height_left = 0, height_right = 0;

    if (left)
    {
        height_left = left->height();
        height_left++;
    }

    if (right)
    {
        height_right = right->height();
        height_right++;
    }

    return std::max(height_left, height_right);
}

template <typename alphabet_T>
XLib::XKC<alphabet_T>::BinaryTree::BinaryTree() : root(new Node())
{
    root->root = root;
}

/**
 * The tree assumes that the alphabet was sorted
 * so the insertion of the most frequebnt value is the highest
 * in the tree
 */
template <typename alphabet_T>
void XLib::XKC<alphabet_T>::BinaryTree::insert(shared_node parent,
                                               alphabet_T value)
{
    if (!parent->left)
    {
        parent->left = shared_node(
          new Node({ parent->root, parent, value }));
        return;
    }
    else if (!parent->right)
    {
        parent->right = shared_node(
          new Node({ parent->root, parent, value }));
        return;
    }
    else if (parent->left->count_nodes() <= parent->right->count_nodes())
    {
        insert(parent->left, value);
    }
    else
    {
        insert(parent->right, value);
    }
}

template <typename alphabet_T>
void XLib::XKC<alphabet_T>::BinaryTree::insert(alphabet_T value)
{
    if (root->value == Node::Value::INVALID)
    {
        root->value = value;
    }
    else
    {
        insert(root, value);
    }
}

template <typename alphabet_T>
XLib::bytes_t XLib::XKC<alphabet_T>::encode(XLib::bytes_t bytes)
{
    return encode(bytes.data(), bytes.size());
}

template <typename alphabet_T>
bool XLib::XKC<alphabet_T>::BinaryTree::path_info(PathInfo& pathInfo,
                                                  shared_node parent,
                                                  alphabet_T value)
{
    if (parent == nullptr)
    {
        return false;
    }

    auto depth = parent->depth();

    if (parent->value == value)
    {
        pathInfo.depth = depth;
        return true;
    }

    /* We've entered in one layer of the tree */
    auto found_left = path_info(pathInfo, parent->left, value);

    if (found_left)
    {
        pathInfo.bit_path[depth] = 0;
        return true;
    }

    pathInfo.bit_path[depth] = 1;

    /* reset depth */
    auto found_right = path_info(pathInfo, parent->right, value);

    if (found_right)
    {
        pathInfo.bit_path[depth] = 1;
        return true;
    }

    pathInfo.bit_path[depth] = 0;

    return found_right;
}

template <typename alphabet_T>
bool XLib::XKC<alphabet_T>::BinaryTree::path_info(PathInfo& pathInfo,
                                                  alphabet_T value)
{
    return path_info(pathInfo, root, value);
}

template <typename alphabet_T>
void XLib::XKC<alphabet_T>::BinaryTree::find_value(
  PathInfoResult& pathInfo)
{
    shared_node current_node = root;

    for (size_t depth = 0; depth < pathInfo.depth; depth++)
    {
        if (pathInfo.bit_path[depth])
        {
            current_node = current_node->right;
        }
        else
        {
            current_node = current_node->left;
        }
    }

    pathInfo.letter_value = current_node->value;
}

template <typename alphabet_T>
std::string XLib::XKC<alphabet_T>::BinaryTree::dot_format(
  shared_node parent)
{
    std::string result;

    auto max_depth_bits = BitsNeeded(parent->root->height());

    if (parent->left)
    {
        result += "\n";
        result += "\"" + std::string(1, parent->value) + " - ";

        std::string depth;

        for (size_t depth_bit = 0; depth_bit < max_depth_bits;
             depth_bit++)
        {
            if (parent->depth() & (1 << depth_bit))
            {
                depth = "1" + depth;
            }
            else
            {
                depth = "0" + depth;
            }
        }

        result += depth;

        for (size_t depth = 0; depth < parent->depth(); depth++)
        {
            result += "x";
        }

        result += "\" -- \"" + std::string(1, parent->left->value)
                  + " - ";

        depth = "";

        for (size_t depth_bit = 0; depth_bit < max_depth_bits;
             depth_bit++)
        {
            if (parent->left->depth() & (1 << depth_bit))
            {
                depth = "1" + depth;
            }
            else
            {
                depth = "0" + depth;
            }
        }

        result += depth;

        for (size_t depth = 0; depth < parent->left->depth(); depth++)
        {
            result += "x";
        }

        result += std::string("\"") + " [label=0]";
        result += dot_format(parent->left);
    }

    if (parent->right)
    {
        result += "\n";
        result += "\"" + std::string(1, parent->value) + " - ";

        std::string depth;

        for (size_t depth_bit = 0; depth_bit < max_depth_bits;
             depth_bit++)
        {
            if (parent->depth() & (1 << depth_bit))
            {
                depth = "1" + depth;
            }
            else
            {
                depth = "0" + depth;
            }
        }

        result += depth;

        for (size_t depth = 0; depth < parent->depth(); depth++)
        {
            result += "x";
        }

        result += "\" -- \"" + std::string(1, parent->right->value)
                  + " - ";

        depth = "";

        for (size_t depth_bit = 0; depth_bit < max_depth_bits;
             depth_bit++)
        {
            if (parent->right->depth() & (1 << depth_bit))
            {
                depth = "1" + depth;
            }
            else
            {
                depth = "0" + depth;
            }
        }

        result += depth;

        for (size_t depth = 0; depth < parent->right->depth(); depth++)
        {
            result += "x";
        }

        result += std::string("\"") + " [label=1]";

        result += dot_format(parent->right);
    }

    return result;
}

template <typename alphabet_T>
std::string XLib::XKC<alphabet_T>::BinaryTree::dot_format()
{
    std::string result = "strict graph {";

    result += dot_format(root);

    result += "\n}";

    return result;
}

template <typename alphabet_T>
XLib::bytes_t XLib::XKC<alphabet_T>::encode(XLib::data_t data,
                                            size_t size)
{
    bytes_t result;
    alphabet_t alphabet;
    occurrences_t occurrences;

    auto values        = view_as<alphabet_T*>(data);
    size_t value_index = 0;

    auto max_values = size / sizeof(alphabet_T);

    /**
     * Store contiguous values
     */
    while (value_index < max_values)
    {
        size_t start_occurrence_index = value_index++;

        Occurrence occurrence;
        occurrence.letter_value = values[start_occurrence_index];
        occurrence.count        = 1;

        for (; value_index < max_values; value_index++)
        {
            if (occurrence.count
                == std::numeric_limits<decltype(Occurrence::count)>::max())
            {
                break;
            }

            if (values[start_occurrence_index] != values[value_index])
            {
                break;
            }

            occurrence.count++;
        }

        occurrences.push_back(occurrence);
    }

    /* Construct the alphabet */
    for (auto&& occurrence : occurrences)
    {
        auto it = std::find_if(alphabet.begin(),
                               alphabet.end(),
                               [&occurrence](Letter& a)
                               {
                                   return (occurrence.letter_value
                                           == a.value);
                               });

        if (it != alphabet.end())
        {
            it->freq += occurrence.count;
        }
        else
        {
            alphabet.push_back(
              { occurrence.letter_value, occurrence.count });
        }
    }

    /* Sort by highest frequency */
    std::sort(alphabet.begin(),
              alphabet.end(),
              [](Letter& a, Letter& b)
              {
                  return (a.freq > b.freq);
              });

    BinaryTree binary_tree;

    for (auto&& letter : alphabet)
    {
        binary_tree.insert(letter.value);
    }

    auto max_tree_depth = binary_tree.root->height();

    auto max_depth_bits = BitsNeeded(max_tree_depth);

    //     auto max_count_occurs = std::max_element(
    //       occurrences.begin(),
    //       occurrences.end(),
    //       [](Occurrence& a, Occurrence& b)
    //       {
    //           return (a.count > b.count);
    //       });

    //     auto max_count_occurs_bits =
    //     BitsNeeded(max_count_occurs->count);

    //     result.push_back(max_count_occurs_bits);

    /**
     * TODO: adjust the encoding size depending on the alphabet_T
     */
    result.push_back(view_as<byte_t>(max_depth_bits));
    /* an alphabet is always bigger than 0 anyway */
    result.push_back(view_as<byte_t>(alphabet.size() - 1));

    for (auto&& letter : alphabet)
    {
        result.push_back(letter.value);
    }

    byte_t result_byte                 = 0;
    size_t written_bits_on_result_byte = 0;
    uint32_t written_bits              = 0;
    //     std::string result_str             = "";

    auto check_bit = [&written_bits_on_result_byte,
                      &result,
                      &result_byte,
                      &written_bits]()
    {
        written_bits_on_result_byte++;
        written_bits++;

        if (written_bits_on_result_byte == 8)
        {
            result.push_back(result_byte);
            written_bits_on_result_byte = 0;
            result_byte                 = 0;
        }
    };

    auto write_bit = [&result_byte, &written_bits_on_result_byte]()
    {
        result_byte |= (1 << written_bits_on_result_byte);
    };

    auto method1 = [&max_depth_bits,
                    &check_bit,
                    &write_bit /*, &result_str*/](PathInfo& path_info)
    {
        //         std::string depth;

        for (size_t depth_bit = 0; depth_bit < max_depth_bits;
             depth_bit++)
        {
            if (path_info.depth & (1 << depth_bit))
            {
                //                 depth = "1" + depth;
                write_bit();
            }
            else
            {
                //                 depth = "0" + depth;
            }

            check_bit();
        }

        //         result_str += depth;

        std::string bit_path;

        for (size_t depth = 0; depth < path_info.depth; depth++)
        {
            if (path_info.bit_path[depth])
            {
                write_bit();
                // bit_path += "1";
            }
            else
            {
                // bit_path += "0";
            }

            //             bit_path += "x";

            check_bit();
        }

        //         result_str += bit_path + " ";
    };

    //     auto method2 = [&result_str, &max_depth_bits, &check_bit,
    //     &write_bit](
    //                      PathInfo& path_info)
    //     {
    //         std::string depth;
    //
    //         if (path_info.depth != 0)
    //         {
    //             for (size_t depth_bit = 0; depth_bit < path_info.depth;
    //                  depth_bit++)
    //             {
    //                 if (path_info.depth & (1 << depth_bit))
    //                 {
    //                     depth += "1";
    //                     write_bit();
    //                 }
    //
    //                 check_bit();
    //             }
    //
    //             depth += "0";
    //             check_bit();
    //         }
    //         else
    //         {
    //             depth = "0" + depth;
    //             check_bit();
    //         }
    //
    //         result_str += depth;
    //
    //         std::string bit_path;
    //
    //         for (size_t depth = 0; depth < path_info.depth; depth++)
    //         {
    //             if (path_info.bit_path[depth])
    //             {
    //                 write_bit();
    //                 bit_path += "1";
    //             }
    //             else
    //             {
    //                 bit_path += "0";
    //             }
    //
    //             check_bit();
    //         }
    //
    //         result_str += bit_path + " ";
    //     };

    for (auto&& occurrence : occurrences)
    {
        PathInfo path_info;
        binary_tree.path_info(path_info, occurrence.letter_value);

        for (size_t count = 0; count < occurrence.count; count++)
        {
            method1(path_info);
        }
    }

    if (written_bits_on_result_byte > 0)
    {
        result.push_back(result_byte);
    }

    for (size_t i = 0; i < sizeof(uint32_t); i++)
    {
        auto bytes_written_bits = view_as<byte_t*>(&written_bits);
        result.push_back(bytes_written_bits[i]);
    }

    //     std::cout << binary_tree.dot_format() << std::endl;
    //     std::cout << result_str << std::endl;
    //     std::cout << written_bits << std::endl;

    //     result_byte                 = 0;
    //     written_bits_on_result_byte = 0;
    //     written_bits                = 0;
    //     result_str                  = "";
    //
    //     for (auto&& occurrence : occurrences)
    //     {
    //         for (size_t count = 0; count < occurrence.count; count++)
    //         {
    //             PathInfo path_info;
    //             binary_tree.path_info(path_info,
    //             occurrence.letter_value);
    //
    //             method2(path_info);
    //         }
    //     }
    //
    //     std::cout << result_str << std::endl;
    //     std::cout << written_bits << std::endl;

    return result;
}

template <typename alphabet_T>
XLib::bytes_t XLib::XKC<alphabet_T>::decode(XLib::bytes_t bytes)
{
    return decode(bytes.data(), bytes.size());
}

template <typename alphabet_T>
XLib::bytes_t XLib::XKC<alphabet_T>::decode(XLib::data_t data,
                                            size_t size)
{
    bytes_t result;

    size_t read_bytes = 0;

    auto read_byte = [&read_bytes, &data]()
    {
        return data[read_bytes++];
    };

    auto max_depth_bits = view_as<size_t>(read_byte());
    auto alphabet_size  = view_as<size_t>(read_byte()) + 1;
    auto written_bits   = *view_as<uint32_t*>(view_as<uintptr_t>(data)
                                            + size - sizeof(uint32_t));

    alphabet_t alphabet;

    for (size_t alphabet_index = 0; alphabet_index < alphabet_size;
         alphabet_index++)
    {
        alphabet.push_back({ read_byte() });
    }

    BinaryTree binary_tree;

    /* Construct the tree */
    for (auto&& letter : alphabet)
    {
        binary_tree.insert(letter.value);
    }

    //     std::cout << binary_tree.dot_format() << std::endl;

    size_t read_bits_on_result_byte = 0;
    size_t bits_read                = 0;
    occurrences_t occurrences;

    while (bits_read < written_bits)
    {
        auto read_bit =
          [&read_bytes, &read_bits_on_result_byte, &bits_read, &data]()
        {
            auto value = (data[read_bytes]
                          & (1 << read_bits_on_result_byte)) ?
                           1 :
                           0;

            read_bits_on_result_byte++;
            bits_read++;

            if (read_bits_on_result_byte == 8)
            {
                read_bytes++;
                read_bits_on_result_byte = 0;
            }

            return value;
        };

        PathInfoResult path_info;

        //         std::string depth;

        for (size_t depth_bit = 0; depth_bit < max_depth_bits;
             depth_bit++)
        {
            auto bit = read_bit();

            //             depth = (bit ? "1" : "0") + depth;

            path_info.depth |= (bit << depth_bit);
        }

        //         std::cout << depth;

        for (size_t depth = 0; depth < path_info.depth; depth++)
        {
            auto bit = read_bit();

            //             std::cout << "x";

            path_info.bit_path[depth] = bit;
        }

        binary_tree.find_value(path_info);

        occurrences.push_back({ path_info.letter_value, 1 });

        //         std::cout << " ";
    }

    //     std::cout << std::endl;

    for (auto&& occurence : occurrences)
    {
        result.push_back(occurence.letter_value);
    }

    return result;
}

#endif

#pragma once

#include <string>
#include <vector>

// namespace for notbmk`s util
namespace ntl
{

namespace cmd
{

struct cmdp_error : std::exception
{
    cmdp_error(const std::string& msg) : msg(msg) { }
    ~cmdp_error() { }
    std::string msg;
    const char* what() const noexcept { return msg.c_str(); };
};

// ignore case, cast ASSCII[32, 126] to index[0, 68]
template <typename _Char = char>
struct char_hash_ignore_case
{
    constexpr int operator()(_Char ch) const
    {
        if (32 <= ch && ch < 127)
        {
            if ('A' <= ch)
            {
                if (ch <= 'Z') ch |= 32;
                ch -= 26;
            }
            return ch - 32;
        }
        else return 0;
    }
};

template <typename _Char, typename _Char_to_Index>
constexpr size_t get_max_index()
{
    _Char_to_Index ctoi;
    size_t maxi = 0;
    size_t maxc = 1 << (sizeof(_Char) * 8 - 1);

    for (size_t c = 0; c < maxc; ++c)
    {
        size_t i = ctoi(c);
        maxi = maxi < i ? i : maxi;
    }
    return maxi;
}

template <typename _Char, typename _Char_to_Index, size_t _Max_index = get_max_index<_Char, _Char_to_Index>() + 1>
class basic_cmd_parser
{
protected:
    // function pointer to char hash
    using char_type = _Char;
    using ctoi_t    = _Char_to_Index;
    using f_noarg_t = void(*)();
    using lpctstr_t = const _Char *;

    struct handler_t
    {
        void operator()(lpctstr_t str) { handle(str); }
        virtual void handle(lpctstr_t str) {  }
    };

    template <typename _Read>
    struct string_reader : public handler_t
    {
        string_reader(_Read reader) : reader(reader) { }
        void handle(lpctstr_t str)
        {
            reader(str);
        }
        _Read reader;
    };

    struct callback_t
    {
        void operator()() { call(); }
        virtual void call() { };
    };

    // function without arguments
    struct function_wa : callback_t
    {
        function_wa(f_noarg_t callback) : callback(callback) { }

        void call() override { callback(); }

    private:

        f_noarg_t callback;
    };

    struct set_flag_to : callback_t
    {
        set_flag_to(bool* flag, bool val = true) : flag(flag), val(val) { }

        void call() override { if (flag) *flag = val; }

    private:

        bool* flag;
        bool val;
    };

    struct Node
    {
        enum Type
        {
            ILLEGAL = -1,
            CALLBACK,
            HANDLER,
        };

        explicit Node()
        {
            this->type = ILLEGAL;
            this->call = nullptr;
            this->what = nullptr;

            for (size_t i = 0; i < _Max_index; ++i)
            {
                this->next[i] = nullptr;
            }
        }

        virtual ~Node()
        {
            for (size_t i = 0; i < _Max_index; ++i)
            {
                if (this->next[i])
                {
                    delete this->next[i];
                }
            }
        }

        bool on_match(lpctstr_t opt, lpctstr_t res)
        {
            switch (this->type)
            {
            case Node::CALLBACK:
                if (call && !*res)
                {
                    call->call();
                    return true;
                }
                break;

            case Node::HANDLER:
                if (handle && *res)
                {
                    handle->handle(res);
                    return true;
                }
                break;
            
            default:
                return false;
            }
            return false;
        }

        Type            type;
        Node*           next[_Max_index];
        union
        {
            callback_t* call;
            handler_t*  handle;
        };
        lpctstr_t        what;
    };

private:

    struct parse_impl
    {
    public:

        parse_impl(basic_cmd_parser* parent, Node* last)
            : parent(parent)
            , last(last) { }

        parse_impl& alias(lpctstr_t option)
        {
            Node* node = parent->_M_insert(option);
            node->type = last->type;
            node->call = last->call;
            return *this;
        }

    private:

        basic_cmd_parser*   parent;
        Node*               last;
    };

public:

    // I`m lazy
    basic_cmd_parser(const basic_cmd_parser&) = delete;
    basic_cmd_parser(basic_cmd_parser&&) = delete;
    basic_cmd_parser& operator=(const basic_cmd_parser&) = delete;
    basic_cmd_parser& operator=(basic_cmd_parser&&) = delete;

    basic_cmd_parser()
    {
        _M_root = _M_addNode(new Node{});
        _M_default_handler = nullptr;
    }

    ~basic_cmd_parser()
    {
        _M_deallocNode(&_M_root);
        for (callback_t* cal : _M_maned_callback)
        {
            delete cal;
        }
        for (handler_t* han : _M_maned_handler)
        {
            delete han;
        }
    }

    int operator[](_Char ch)
    {
        return _M_ctoi(ch);
    }

    lpctstr_t recent()
    {
        return _M_root->what;
    }

    /**
     * @param flag would set to val
     */ 
    parse_impl flag(lpctstr_t option, bool* flag, bool val = true)
    {
        return bind(option, new set_flag_to(flag, val), true);
    }

    // @param call will be called when matched
    parse_impl add(lpctstr_t option, f_noarg_t call)
    {
        return bind(option, new function_wa(call), true);
    }

    // handle string after the option
    // for example: "-version=1.20", the handler will get "1.20"
    template <typename _Handler>
    parse_impl gets(lpctstr_t option, _Handler handler)
    {
        return bind(option, new string_reader(handler), true);
    }

    template <typename _Handler>
    void set_default(_Handler handler)
    {
        if (_M_default_handler)
        {
            delete _M_default_handler;
        }
        _M_default_handler = new string_reader(handler);
    }

    /**
     * @param man if set true, 'res' would be released inside the class
     */
    parse_impl bind(lpctstr_t option, callback_t* cal, bool man = false)
    {
        Node* node = _M_insert(option);
        node->type = Node::CALLBACK;
        if (man) node->call = _M_add_callback(cal);
        else node->call = cal;
        return parse_impl(this, node);
    }

    /**
     * @param man if set true, 'res' would be released inside the class
     */
    parse_impl bind(lpctstr_t option, handler_t* han, bool man = false)
    {
        Node* node = _M_insert(option);
        node->type = Node::HANDLER;
        if (man) node->handle = _M_add_handler(han);
        else node->handle = han;
        return parse_impl(this, node);
    }

    /** parse once
     */
    void parse(lpctstr_t str)
    {
        if (*str != '-' && _M_default_handler)
        {
            _M_default_handler->handle(str);
            return;
        }
        Node* node;
        lpctstr_t res = _M_walk(str, &node);
        if(!node->on_match(str, res))
        {
            char buf[256];
            if (recent())
            {
                sprintf(buf, "invalid option: \"%s\", did you mean \"%s\" ?", str, recent());
                throw cmdp_error(buf);
            }
            else
            {
                sprintf(buf, "invalid option: \"%s\"", str);
                throw cmdp_error(buf);
            }
        }
    }

    /**
     * parse option from main('argc', 'argv')
     */
    void parse(int argc, lpctstr_t argv[])
    {
        for (int argi = 1; argi < argc; ++argi)
        {
            try
            {
                parse(argv[argi]);
            }
            catch(const cmdp_error& e)
            {
                std::cerr << e.what() << '\n';
            }
        }
    }

protected:

    lpctstr_t _M_walk(lpctstr_t str, Node** ret)
    {
        _M_root->what = nullptr;
        Node* node = _M_root;
        while(*str && node->next[_M_ctoi(*str)])
        {
            if (node->what) _M_root->what = node->what;
            node = node->next[_M_ctoi(*str)];
            ++str;
        }
        if (ret) *ret = node;
        return str;
    }

    Node* _M_insert(lpctstr_t str)
    {
        Node* node;
        lpctstr_t res = _M_walk(str, &node);
        node = _M_insert_after(node, res);
        if (node->what) { throw cmdp_error("multiple definition"); }
        node->what = str;
        return node;
    }

    Node* _M_insert_after(Node* node, lpctstr_t str)
    {
        while(*str)
        {
            Node* next = _M_addNode(new Node{});
            node->next[_M_ctoi(*str)] = next;
            node = next;
            ++str;
        }
        return node;
    }

    Node* _M_addNode(Node* node)
    {
        return node;
    }

    void _M_deallocNode(Node** node)
    {
        delete *node;
        *node = nullptr;
    }

    callback_t* _M_add_callback(callback_t* ptr)
    {
        _M_maned_callback.push_back(ptr);
        return ptr;
    }

    handler_t* _M_add_handler(handler_t* ptr)
    {
        _M_maned_handler.push_back(ptr);
        return ptr;
    }

protected:

    Node*                       _M_root;
    handler_t*                  _M_default_handler;
    ctoi_t                      _M_ctoi;
    std::vector<callback_t*>    _M_maned_callback;
    std::vector<handler_t*>     _M_maned_handler;
};

// command option parser
typedef basic_cmd_parser<char, char_hash_ignore_case<char>> cmdp;

} // ! cmd

} // ! parse
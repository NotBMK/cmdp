#ifndef NTL__CMDP__H
#define NTL__CMDP__H

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

template <typename _Char, typename _Char_to_Index, size_t _Index_Count = get_max_index<_Char, _Char_to_Index>() + 1>
class basic_cmd_parser
{
protected:
    // function pointer to char hash
    using char_type = _Char;
    using ctoi_t    = _Char_to_Index;
    using f_noarg_t = void(*)();
    using lpctstr_t = const _Char *;
    using string  = std::basic_string<_Char>;

    struct basic_handler
    {
        void operator()(lpctstr_t str) { handle(str); }
        virtual void handle(lpctstr_t str) {  }
    };

    template <typename _Read>
    struct string_handler : public basic_handler
    {
        string_handler(_Read reader) : reader(reader) { }
        void handle(lpctstr_t str)
        {
            reader(str);
        }
        _Read reader;
    };

    struct basic_callback
    {
        void operator()() { call(); }
        virtual void call() { };
    };

    // function without arguments
    struct function_wa : basic_callback
    {
        function_wa(f_noarg_t callback) : callback(callback) { }

        void call() override { callback(); }

    private:

        f_noarg_t callback;
    };

    struct set_flag_to : basic_callback
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

            for (size_t i = 0; i < _Index_Count; ++i)
            {
                this->next[i] = nullptr;
            }
        }

        virtual ~Node()
        {
            for (size_t i = 0; i < _Index_Count; ++i)
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
            }
            return false;
        }

        Type                type;
        Node*               next[_Index_Count];
        union
        {
            basic_callback*     call;
            basic_handler*      handle;
        };
        lpctstr_t           what;
    };

private:

    struct cmdp_impl
    {
    public:

        cmdp_impl(basic_cmd_parser* parent, Node* last)
            : parent(parent)
            , last(last) { }

        cmdp_impl& alias(lpctstr_t option)
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
        for (basic_callback* cal : _M_maned_callback)
        {
            delete cal;
        }
        for (basic_handler* han : _M_maned_handler)
        {
            delete han;
        }
    }

    int operator[](_Char ch)
    {
        return _M_ctoi(ch);
    }

    lpctstr_t last()
    {
        return _M_root->what;
    }

    /**
     * @param flag would set to val
     */ 
    cmdp_impl flag(lpctstr_t option, bool* flag, bool val = true)
    {
        return bind(option, new set_flag_to(flag, val), true);
    }

    // @param call will be called when matched
    cmdp_impl add(lpctstr_t option, f_noarg_t call)
    {
        return bind(option, new function_wa(call), true);
    }

    // handle string after the option
    // for example: "-version=1.20", the handler will get "1.20"
    template <typename _Handler>
    cmdp_impl gets(lpctstr_t option, _Handler handler)
    {
        return bind(option, new string_handler(handler), true);
    }

    template <typename _Handler>
    void set_default(_Handler handler)
    {
        if (_M_default_handler)
        {
            delete _M_default_handler;
        }
        _M_default_handler = new string_handler(handler);
    }

    /**
     * @param man if set true, 'res' would be released inside the class
     */
    cmdp_impl bind(lpctstr_t option, basic_callback* cal, bool man = false)
    {
        Node* node = _M_insert(option);
        node->type = Node::CALLBACK;
        if (man) node->call = _M_add_callback(cal);
        else node->call = cal;
        return cmdp_impl(this, node);
    }

    /**
     * @param man if set true, 'res' would be released inside the class
     */
    cmdp_impl bind(lpctstr_t option, basic_handler* han, bool man = false)
    {
        Node* node = _M_insert(option);
        node->type = Node::HANDLER;
        if (man) node->handle = _M_add_handler(han);
        else node->handle = han;
        return cmdp_impl(this, node);
    }

    /** parse once
     */
    void _M_parse_once(lpctstr_t str)
    {
        Node* node;
        lpctstr_t res = _M_walk(str, &node);
        if (node == _M_root)
        {
            _M_default_handler->handle(str);
            return;
        }
        if (!node->on_match(str, res))
        {
            char buf[256];
            if (last())
            {
                sprintf(buf, "invalid option: \"%s\", did you mean \"%s\" ?", str, last());
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
                _M_parse_once(argv[argi]);
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

    basic_callback* _M_add_callback(basic_callback* ptr)
    {
        _M_maned_callback.push_back(ptr);
        return ptr;
    }

    basic_handler* _M_add_handler(basic_handler* ptr)
    {
        _M_maned_handler.push_back(ptr);
        return ptr;
    }

protected:

    Node*                       _M_root;
    basic_handler*                  _M_default_handler;
    ctoi_t                      _M_ctoi;
    std::vector<basic_callback*>    _M_maned_callback;
    std::vector<basic_handler*>     _M_maned_handler;
};

// command option parser
typedef basic_cmd_parser<char, char_hash_ignore_case<char>> cmdp;

} // cmd

} // ntl

#endif
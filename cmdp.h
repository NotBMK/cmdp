#ifndef NTL__CMDP__H
#define NTL__CMDP__H

#include <string>
#include <vector>
#include <functional>

// namespace for notbmk`s util
namespace ntl
{

namespace cmd
{

struct cmdp_error : std::exception
{
    cmdp_error(const std::string& msg) : msg(msg) { }
    cmdp_error(std::string&& msg) : msg(std::move(msg)) { }
    ~cmdp_error() { }
    std::string msg;
    const char* what() const noexcept { return msg.c_str(); };
};

// ignore case, cast ASSCII[32, 126] to index[0, 68]
template <typename _Char = char>
struct char_hash_ignore_case
{
    constexpr int
        operator()(_Char ch) const
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

    using char_type = _Char;
    using ctoi_t    = _Char_to_Index;
    using f_noarg_t = std::function<void()>;
    using lpctstr_t = const char_type *;
    using string_t  = std::basic_string<char_type>;
    using sstream_t = std::basic_stringstream<char_type>;

    struct basic_target
    {
        virtual bool invoke() { return false; }
        virtual bool handle(lpctstr_t str) { return false; }
    };

    class callback : public basic_target
    {
    public:

        callback(f_noarg_t f) :f(f) { }
        bool invoke() { f(); return true; }

    private:

        f_noarg_t f;
    };

    struct char_node
    {
        explicit char_node()
        {
            this->what = nullptr;
            this->target = nullptr;

            for (size_t i = 0; i < _Index_Count; ++i)
            {
                this->next[i] = nullptr;
            }
        }

        virtual ~char_node()
        {
            for (size_t i = 0; i < _Index_Count; ++i)
            {
                if (this->next[i]) delete this->next[i];
            }
            if (what) delete what;
        }

        /**
         * Bind a string to the current node.
         * ---- Consider using std::optional 
         */
        void bind(lpctstr_t str)
        {
            if (str)
            {
                if (!what) what = new string_t;
                *what = str;
            }
            else
            {
                if (what) { delete what; what = nullptr; }
            }
        }

        char_node*      next[_Index_Count];
        string_t*       what;
        basic_target*   target;
    };

private:

    struct cmdp_impl
    {
    public:

        cmdp_impl(basic_cmd_parser* parent, char_node* last)
            : parent(parent)
            , vector({last})
            , target(nullptr) { }

        ~cmdp_impl()
        {
            if (target)
            {
                parent->_M_add_target(target);
                for (char_node* node : vector)
                {
                    node->target = target;
                }
            }
        }

        /**
         * Alias the current option.
         */
        cmdp_impl& alias(lpctstr_t option)
        {
            char_node* node = parent->_M_insert(parent->_M_root, option);
            vector.push_back(node);
            return *this;
        }

        /**
         * Bind a callback function to the current option.
         */
        cmdp_impl& bind(f_noarg_t call)
        {
            if (!target) target = new callback(call);
            else throw cmdp_error("multiple bind.");
            return *this;
        }

    private:

        basic_cmd_parser*           parent;
        std::vector<char_node*>     vector;
        basic_target*               target;
    };

public:

    basic_cmd_parser(const basic_cmd_parser&)               = delete;
    basic_cmd_parser(basic_cmd_parser&&)                    = delete;
    basic_cmd_parser& operator=(const basic_cmd_parser&)    = delete;
    basic_cmd_parser& operator=(basic_cmd_parser&&)         = delete;

    basic_cmd_parser()
    {
        _M_root = _M_add_node(new char_node{});
        _M_argi = 0;
        _M_argv = nullptr;
    }

    ~basic_cmd_parser()
    {
        _M_root->what = nullptr;
        _M_dealloc_node(&_M_root);
        for (basic_target* target : _M_targets)
        {
            delete target;
        }
    }

    /**
     * @return next string
     */
    lpctstr_t next_str() { lpctstr_t ret = _M_get_next_str(); _M_next(); return ret; }

    /**
     * @return next integer (long)
     */
    long next_int() { return _M_next_value<long>(); }

    /**
     * @return next double
     */
    double next_double() { return _M_next_value<double>(); }

    /**
     * @return The last matched option.
     */
    lpctstr_t last() { return _M_root->what ? _M_root->what->c_str() : nullptr; }

    /**
     * Add the option
     */
    cmdp_impl add(lpctstr_t option)
    {
        char_node* node = _M_insert(_M_root, option);
        return cmdp_impl(this, node);
    }

    /**
     * 
     */
    void init(int argc, lpctstr_t argv[])
    {
        _M_argi = 1;
        _M_argc = argc;
        _M_argv = argv;
    }

    /**
     * parse options got from init('argc', 'argv')
     */
    void parse()
    {
        while (_M_parsing())
        {
            try
            {
                _M_parse_once(_M_get_now_str());              
            }
            catch(const cmdp_error& e)
            {
                std::cerr << e.what() << '\n';
            }
            _M_next();
        }
    }

protected:

    bool _M_parsing() { return _M_argv ? (_M_argi < _M_argc) : false; }

    bool _M_has_next() { return _M_argv ? (1 + _M_argi < _M_argc) : false; }
    
    void _M_next() { ++_M_argi; }

    template <typename _Type>
    _Type _M_next_value()
    {
        sstream_t ss(_M_get_next_str()); _M_next();
        _Type ret;
        if(!(ss >> ret))
        {
            char buf[256];
            if (last())
                sprintf(buf, "%s got a wrong value", last());
            else
                sprintf(buf, "processing \"%s\" failed", _M_get_now_str());
            throw cmdp_error(buf);
        }
        return ret;
    }

    lpctstr_t _M_get_now_str() { return _M_parsing() ? _M_argv[_M_argi] : nullptr; }

    lpctstr_t _M_get_next_str()
    {
        if (_M_has_next())
        {
            return _M_argv[1 + _M_argi];
        }
        else throw cmdp_error("no more argument.");
    }

    /**
     * Traverse along the character tree.
     * @param ret return the string not in the character tree
     */
    char_node* _M_walk(char_node* start, lpctstr_t str, lpctstr_t* ret)
    {
        _M_root->what = nullptr;
        char_node* node = start;
        while(*str && node->next[_M_ctoi(*str)])
        {
            node = node->next[_M_ctoi(*str)];
            if (node->what) _M_root->what = node->what;
            ++str;
        }
        if (ret) *ret = str;
        return node;
    }

    char_node* _M_insert(char_node* root, lpctstr_t str)
    {
        lpctstr_t res;
        char_node* node = _M_walk(root, str, &res);
        node = _M_insert_after(node, res);
        if (node->what) { throw cmdp_error("multiple definition"); }
        node->bind(str);
        return node;
    }

    /**
     * TODO: char_node* _M_insert_after(char_node*, char_node*)
     */
    char_node* _M_insert_after(char_node* node, lpctstr_t str)
    {
        while(*str)
        {
            char_node* next = _M_add_node(new char_node{});
            node->next[_M_ctoi(*str)] = next;
            node = next;
            ++str;
        }
        return node;
    }

    bool _M_is_option(lpctstr_t str)
    {
        return *str == (char_type)'-';
    }

    /**
     * Verify if the str is one of the given options.
     */
    bool _M_verify_option(lpctstr_t str)
    {
        char_node* node = _M_walk(_M_root, str, 0);
        return node->what;
    }

    /** 
     * parse once
     */
    void _M_parse_once(lpctstr_t str)
    {
        if (!str) throw cmdp_error("got null str");
        lpctstr_t res;
        char_node* node  = _M_walk(_M_root, str, &res);
        bool is_good = (!*res && node->what && node->target && (node->target->invoke() || node->target->handle(res)));
        if (!is_good)
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
     * use for debug
     */
    char_node* _M_add_node(char_node* node) { return node; }

    basic_target* _M_add_target(basic_target* t) { _M_targets.push_back(t); return t; }

    void _M_dealloc_node(char_node** node) { delete *node; *node = nullptr; }

protected:

    int                         _M_argi;
    int                         _M_argc;
    lpctstr_t*                  _M_argv;

    char_node*                  _M_root;
    ctoi_t                      _M_ctoi;
    std::vector<basic_target*>  _M_targets;
};

// command option parser
typedef basic_cmd_parser<char, char_hash_ignore_case<char>> cmdp;

} // cmd

} // ntl

#endif
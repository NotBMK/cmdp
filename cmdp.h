#ifndef NTL__CMDP__H
#define NTL__CMDP__H

#include <string>
#include <vector>
#include <optional>
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
class basic_cmdp
{
protected:

    using char_type     = _Char;
    using ctoi_t        = _Char_to_Index;
    using f_noarg_t     = std::function<void()>;
    using lpctstr_t     = const char_type *;
    using string_t      = std::basic_string<char_type>;
    using sstream_t     = std::basic_stringstream<char_type>;

    struct arg_iter
    {
        void init(int c, lpctstr_t* v) { argi = 0; argc = c; argv = v; }
        bool good(int offset = 0) { int i = offset + argi; return 0 <= i && i < argc; }
        operator bool() { return good(0); }

        lpctstr_t now() { return good(0) ? argv[argi] : nullptr; }
        lpctstr_t next() { return good(1) ? argv[1 +  argi] : nullptr; }
        void step() { ++argi; }
        void reset() { argi = 0; }

        int         argi = 0;
        int         argc;
        lpctstr_t*  argv = nullptr;
    };

    struct basic_target
    {
        virtual ~basic_target() { }
        virtual bool invoke() { return false; }
        virtual bool handle(lpctstr_t str) { return false; }
    };

    class callback final : public basic_target
    {
    public:

        callback(f_noarg_t f) :f(f) { }
        bool invoke() { f(); return true; }

    private:

        f_noarg_t f;
    };

    template <typename T>
    class setter final : public basic_target
    {
    public:
        setter(T* target, const T& value) : target(target), value(value) { }
        bool invoke() override { *target = value; return true; }

    private:

        T*          target;
        const T&    value;
    };

    struct char_node
    {
        explicit char_node()
        {
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
        }

        /**
         * Bind a string to the current node.
         * ---- Consider using std::optional 
         */
        void bind(lpctstr_t str) { if (str) what = str; else what.reset(); }

        char_node*                      next[_Index_Count];
        std::optional<string_t>         what;
        basic_target*                   target;
    };

private:

    struct cmdp_impl
    {
    public:

        cmdp_impl(basic_cmdp* parent, char_node* last)
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

        /**
         * 
         */
        template <typename T>
        cmdp_impl& bind(T* t, const T& value)
        {
            if (!target) target = new setter<T>(t, value);
            else throw cmdp_error("multiple bind.");
            return *this;
        }

    private:

        basic_cmdp*                 parent;
        std::vector<char_node*>     vector;
        basic_target*               target;
    };

public:

    basic_cmdp(const basic_cmdp&)               = delete;
    basic_cmdp(basic_cmdp&&)                    = delete;
    basic_cmdp& operator=(const basic_cmdp&)    = delete;
    basic_cmdp& operator=(basic_cmdp&&)         = delete;

    basic_cmdp()
    {
        _M_root = _M_add_node(new char_node{});
    }

    ~basic_cmdp()
    {
        _M_root->what.reset();
        _M_dealloc_node(&_M_root);
        for (basic_target* target : _M_targets)
        {
            delete target;
        }
    }

    /**
     * @return next string
     */
    lpctstr_t next_str() { lpctstr_t ret = _M_args.next(); _M_args.step(); return ret; }

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
        _M_args.init(argc, argv);
    }

    /**
     * parse options got from init('argc', 'argv')
     */
    void parse()
    {
        while (_M_args.good(0))
        {
            try
            {
                _M_parse_once(_M_args.now());              
            }
            catch(const cmdp_error& e)
            {
                std::cerr << e.what() << '\n';
            }
            _M_args.step();
        }
    }

protected:

    /**
     * do nothing
     */
    static basic_target* nulltarget() { static basic_target null; return &null; }

    /**
     * Traverse along the character tree.
     * @param ret return the string not in the character tree
     */
    char_node* _M_walk(char_node* start, lpctstr_t str, lpctstr_t* ret)
    {
        _M_root->what.reset();
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

    /**
     * 
     */
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

    arg_iter                    _M_args;

    char_node*                  _M_root;
    ctoi_t                      _M_ctoi;
    std::vector<basic_target*>  _M_targets;
};

// command option parser
typedef basic_cmdp<char, char_hash_ignore_case<char>> cmdp;

} // cmd

} // ntl

#endif
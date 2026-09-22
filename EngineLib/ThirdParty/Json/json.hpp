
#pragma once

#include <cstdint>
#include <charconv>
#include <limits>
#include <stdexcept>
#include <cmath>
#include <cctype>
#include <string>
#include <deque>
#include <map>
#include <type_traits>
#include <initializer_list>
#include <ostream>
#include <iostream>

namespace json {

using std::map;
using std::deque;
using std::string;
using std::enable_if;
using std::initializer_list;
using std::is_same;
using std::is_convertible;
using std::is_integral;
using std::is_floating_point;

namespace {
    string json_escape( const string &str ) {
        string output;
        for( unsigned i = 0; i < str.length(); ++i )
            switch( str[i] ) {
                case '\"': output += "\\\""; break;
                case '\\': output += "\\\\"; break;
                case '\b': output += "\\b";  break;
                case '\f': output += "\\f";  break;
                case '\n': output += "\\n";  break;
                case '\r': output += "\\r";  break;
                case '\t': output += "\\t";  break;
                default  :
                    if (static_cast<unsigned char>(str[i]) < 0x20) {
                        const char* hex = "0123456789abcdef";
                        output += "\\u00";
                        output += hex[(static_cast<unsigned char>(str[i]) >> 4) & 15];
                        output += hex[static_cast<unsigned char>(str[i]) & 15];
                    } else output += str[i];
                    break;
            }
        return std::move( output );
    }
}

class JSON
{
    union BackingData {
        BackingData( double d ) : Float( d ){}
        BackingData( long   l ) : Int( l ){}
        BackingData( bool   b ) : Bool( b ){}
        BackingData( string s ) : String( new string( s ) ){}
        BackingData()           : Int( 0 ){}

        deque<JSON>        *List;
        map<string,JSON>   *Map;
        string             *String;
        double              Float;
        long                Int;
        bool                Bool;
    } Internal;

    public:
        enum class Class {
            Null,
            Object,
            Array,
            String,
            Floating,
            Integral,
            Boolean
        };

        template <typename Container>
        class JSONWrapper {
            Container *object;

            public:
                JSONWrapper( Container *val ) : object( val ) {}
                JSONWrapper( std::nullptr_t )  : object( nullptr ) {}

                typename Container::iterator begin() { return object ? object->begin() : typename Container::iterator(); }
                typename Container::iterator end() { return object ? object->end() : typename Container::iterator(); }
                typename Container::const_iterator begin() const { return object ? object->begin() : typename Container::iterator(); }
                typename Container::const_iterator end() const { return object ? object->end() : typename Container::iterator(); }
        };

        template <typename Container>
        class JSONConstWrapper {
            const Container *object;

            public:
                JSONConstWrapper( const Container *val ) : object( val ) {}
                JSONConstWrapper( std::nullptr_t )  : object( nullptr ) {}

                typename Container::const_iterator begin() const { return object ? object->begin() : typename Container::const_iterator(); }
                typename Container::const_iterator end() const { return object ? object->end() : typename Container::const_iterator(); }
        };

        JSON() : Internal(), Type( Class::Null ){}

        JSON( initializer_list<JSON> list ) 
            : JSON() 
        {
            SetType( Class::Object );
            for( auto i = list.begin(), e = list.end(); i != e; ++i, ++i )
                operator[]( i->ToString() ) = *std::next( i );
        }

        JSON( JSON&& other )
            : Internal( other.Internal )
            , Type( other.Type )
        { other.Type = Class::Null; other.Internal.Map = nullptr; }

        JSON& operator=( JSON&& other ) {
            ClearInternal();
            Internal = other.Internal;
            Type = other.Type;
            other.Internal.Map = nullptr;
            other.Type = Class::Null;
            return *this;
        }

        JSON( const JSON &other ) {
            switch( other.Type ) {
            case Class::Object:
                Internal.Map = 
                    new map<string,JSON>( other.Internal.Map->begin(),
                                          other.Internal.Map->end() );
                break;
            case Class::Array:
                Internal.List = 
                    new deque<JSON>( other.Internal.List->begin(),
                                      other.Internal.List->end() );
                break;
            case Class::String:
                Internal.String = 
                    new string( *other.Internal.String );
                break;
            default:
                Internal = other.Internal;
            }
            Type = other.Type;
        }

        JSON& operator=( const JSON &other ) {
            ClearInternal();
            switch( other.Type ) {
            case Class::Object:
                Internal.Map = 
                    new map<string,JSON>( other.Internal.Map->begin(),
                                          other.Internal.Map->end() );
                break;
            case Class::Array:
                Internal.List = 
                    new deque<JSON>( other.Internal.List->begin(),
                                      other.Internal.List->end() );
                break;
            case Class::String:
                Internal.String = 
                    new string( *other.Internal.String );
                break;
            default:
                Internal = other.Internal;
            }
            Type = other.Type;
            return *this;
        }

        ~JSON() {
            switch( Type ) {
            case Class::Array:
                delete Internal.List;
                break;
            case Class::Object:
                delete Internal.Map;
                break;
            case Class::String:
                delete Internal.String;
                break;
            default:;
            }
        }

        template <typename T>
        JSON( T b, typename enable_if<is_same<T,bool>::value>::type* = 0 ) : Internal( b ), Type( Class::Boolean ){}

        template <typename T>
        JSON( T i, typename enable_if<is_integral<T>::value && !is_same<T,bool>::value>::type* = 0 ) : Internal( (long)i ), Type( Class::Integral ){}

        template <typename T>
        JSON( T f, typename enable_if<is_floating_point<T>::value>::type* = 0 ) : Internal( (double)f ), Type( Class::Floating ){}

        template <typename T>
        JSON( T s, typename enable_if<is_convertible<T,string>::value>::type* = 0 ) : Internal( string( s ) ), Type( Class::String ){}

        JSON( std::nullptr_t ) : Internal(), Type( Class::Null ){}

        static JSON Make( Class type ) {
            JSON ret; ret.SetType( type );
            return ret;
        }

        static inline JSON Load( const string & );

        template <typename T>
        void append( T arg ) {
            SetType( Class::Array ); Internal.List->emplace_back( arg );
        }

        template <typename T, typename... U>
        void append( T arg, U... args ) {
            append( arg ); append( args... );
        }

        template <typename T>
            typename enable_if<is_same<T,bool>::value, JSON&>::type operator=( T b ) {
                SetType( Class::Boolean ); Internal.Bool = b; return *this;
            }

        template <typename T>
            typename enable_if<is_integral<T>::value && !is_same<T,bool>::value, JSON&>::type operator=( T i ) {
                SetType( Class::Integral ); Internal.Int = i; return *this;
            }

        template <typename T>
            typename enable_if<is_floating_point<T>::value, JSON&>::type operator=( T f ) {
                SetType( Class::Floating ); Internal.Float = f; return *this;
            }

        template <typename T>
            typename enable_if<is_convertible<T,string>::value, JSON&>::type operator=( T s ) {
                SetType( Class::String ); *Internal.String = string( s ); return *this;
            }

        JSON& operator[]( const string &key ) {
            SetType( Class::Object ); return Internal.Map->operator[]( key );
        }

        JSON& operator[]( unsigned index ) {
            SetType( Class::Array );
            if( index >= Internal.List->size() ) Internal.List->resize( index + 1 );
            return Internal.List->operator[]( index );
        }

        JSON &at( const string &key ) {
            return operator[]( key );
        }

        const JSON &at( const string &key ) const {
            return Internal.Map->at( key );
        }

        JSON &at( unsigned index ) {
            return operator[]( index );
        }

        const JSON &at( unsigned index ) const {
            return Internal.List->at( index );
        }

        int length() const {
            if( Type == Class::Array )
                return Internal.List->size();
            else
                return -1;
        }

        bool hasKey( const string &key ) const {
            if( Type == Class::Object )
                return Internal.Map->find( key ) != Internal.Map->end();
            return false;
        }

        int size() const {
            if( Type == Class::Object )
                return Internal.Map->size();
            else if( Type == Class::Array )
                return Internal.List->size();
            else
                return -1;
        }

        Class JSONType() const { return Type; }

        /// Functions for getting primitives from the JSON object.
        bool IsNull() const { return Type == Class::Null; }

        string ToString() const { bool b; return std::move( ToString( b ) ); }
        string ToString( bool &ok ) const {
            ok = (Type == Class::String);
            return ok ? std::move( json_escape( *Internal.String ) ): string("");
        }

        const string& ToRawString() const {
            if (Type != Class::String) throw std::runtime_error("Expected JSON string");
            return *Internal.String;
        }

        double ToFloat() const { bool b; return ToFloat( b ); }
        double ToFloat( bool &ok ) const {
            ok = (Type == Class::Floating);
            return ok ? Internal.Float : 0.0;
        }

        long ToInt() const { bool b; return ToInt( b ); }
        long ToInt( bool &ok ) const {
            ok = (Type == Class::Integral);
            return ok ? Internal.Int : 0;
        }

        bool ToBool() const { bool b; return ToBool( b ); }
        bool ToBool( bool &ok ) const {
            ok = (Type == Class::Boolean);
            return ok ? Internal.Bool : false;
        }

        JSONWrapper<map<string,JSON>> ObjectRange() {
            if( Type == Class::Object )
                return JSONWrapper<map<string,JSON>>( Internal.Map );
            return JSONWrapper<map<string,JSON>>( nullptr );
        }

        JSONWrapper<deque<JSON>> ArrayRange() {
            if( Type == Class::Array )
                return JSONWrapper<deque<JSON>>( Internal.List );
            return JSONWrapper<deque<JSON>>( nullptr );
        }

        JSONConstWrapper<map<string,JSON>> ObjectRange() const {
            if( Type == Class::Object )
                return JSONConstWrapper<map<string,JSON>>( Internal.Map );
            return JSONConstWrapper<map<string,JSON>>( nullptr );
        }


        JSONConstWrapper<deque<JSON>> ArrayRange() const { 
            if( Type == Class::Array )
                return JSONConstWrapper<deque<JSON>>( Internal.List );
            return JSONConstWrapper<deque<JSON>>( nullptr );
        }

        string dump( int depth = 1, string tab = "  ") const {
            string pad = "";
            for( int i = 0; i < depth; ++i, pad += tab );

            switch( Type ) {
                case Class::Null:
                    return "null";
                case Class::Object: {
                    string s = "{\n";
                    bool skip = true;
                    for( auto &p : *Internal.Map ) {
                        if( !skip ) s += ",\n";
                        s += ( pad + "\"" + json_escape(p.first) + "\" : " + p.second.dump( depth + 1, tab ) );
                        skip = false;
                    }
                    s += ( "\n" + pad.erase( 0, 2 ) + "}" ) ;
                    return s;
                }
                case Class::Array: {
                    string s = "[";
                    bool skip = true;
                    for( auto &p : *Internal.List ) {
                        if( !skip ) s += ", ";
                        s += p.dump( depth + 1, tab );
                        skip = false;
                    }
                    s += "]";
                    return s;
                }
                case Class::String:
                    return "\"" + json_escape( *Internal.String ) + "\"";
                case Class::Floating:
                    {
                    if (!std::isfinite(Internal.Float)) throw std::runtime_error("Non-finite JSON number");
                    char buffer[64];
                    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), Internal.Float,
                        std::chars_format::general, std::numeric_limits<double>::max_digits10);
                    if (result.ec != std::errc{}) throw std::runtime_error("Cannot write JSON number");
                    string text(buffer, result.ptr);
                    // Preserve Floating when reloaded: existing scene readers distinguish 1 from 1.0.
                    if (text.find_first_of(".eE") == string::npos) text += ".0";
                    return text;
                }
                case Class::Integral:
                    return std::to_string( Internal.Int );
                case Class::Boolean:
                    return Internal.Bool ? "true" : "false";
                default:
                    return "";
            }
            return "";
        }

        friend std::ostream& operator<<( std::ostream&, const JSON & );

    private:
        void SetType( Class type ) {
            if( type == Type )
                return;

            ClearInternal();
          
            switch( type ) {
            case Class::Null:      Internal.Map    = nullptr;                break;
            case Class::Object:    Internal.Map    = new map<string,JSON>(); break;
            case Class::Array:     Internal.List   = new deque<JSON>();     break;
            case Class::String:    Internal.String = new string();           break;
            case Class::Floating:  Internal.Float  = 0.0;                    break;
            case Class::Integral:  Internal.Int    = 0;                      break;
            case Class::Boolean:   Internal.Bool   = false;                  break;
            }

            Type = type;
        }

    private:
      /* beware: only call if YOU know that Internal is allocated. No checks performed here. 
         This function should be called in a constructed JSON just before you are going to 
        overwrite Internal... 
      */
      void ClearInternal() {
        switch( Type ) {
          case Class::Object: delete Internal.Map;    break;
          case Class::Array:  delete Internal.List;   break;
          case Class::String: delete Internal.String; break;
          default:;
        }
      }

    private:

        Class Type = Class::Null;
};

inline JSON Array() {
    return std::move( JSON::Make( JSON::Class::Array ) );
}

template <typename... T>
inline JSON Array( T... args ) {
    JSON arr = JSON::Make( JSON::Class::Array );
    arr.append( args... );
    return std::move( arr );
}

inline JSON Object() {
    return std::move( JSON::Make( JSON::Class::Object ) );
}

inline std::ostream& operator<<( std::ostream &os, const JSON &json ) {
    os << json.dump();
    return os;
}

// Bounded parser: malformed/truncated documents throw instead of reading beyond the input.
// Full precision numbers and decoded Unicode are required by asset metadata round trips.
namespace {
    class Parser {
        const string& Input;
        size_t Pos = 0;
        char Peek() const { return Pos < Input.size() ? Input[Pos] : '\0'; }
        [[noreturn]] void Fail() const { throw std::runtime_error("Invalid JSON at byte " + std::to_string(Pos)); }
        void WS() { while (Peek() == ' ' || Peek() == '\n' || Peek() == '\r' || Peek() == '\t') ++Pos; }
        bool Take(char c) { if (Pos < Input.size() && Input[Pos] == c) { ++Pos; return true; } return false; }
        void Expect(char c) { if (!Take(c)) Fail(); }
        unsigned Hex4() {
            unsigned value = 0;
            for (int i = 0; i < 4; ++i) {
                const char c = Peek(); unsigned digit;
                if (c >= '0' && c <= '9') digit = c - '0';
                else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
                else Fail();
                ++Pos; value = value * 16 + digit;
            }
            return value;
        }
        void Utf8(string& out, unsigned cp) {
            if (cp <= 0x7f) out += char(cp);
            else if (cp <= 0x7ff) { out += char(0xc0 | cp >> 6); out += char(0x80 | (cp & 63)); }
            else if (cp <= 0xffff) {
                out += char(0xe0 | cp >> 12); out += char(0x80 | ((cp >> 6) & 63)); out += char(0x80 | (cp & 63));
            } else {
                out += char(0xf0 | cp >> 18); out += char(0x80 | ((cp >> 12) & 63));
                out += char(0x80 | ((cp >> 6) & 63)); out += char(0x80 | (cp & 63));
            }
        }
        string String() {
            Expect('"'); string out;
            while (!Take('"')) {
                if (Pos >= Input.size()) Fail();
                const auto c = static_cast<unsigned char>(Input[Pos++]);
                if (c < 0x20) Fail();
                if (c != '\\') {
                    if (c < 0x80) { out += char(c); continue; }
                    unsigned cp, count, minimum;
                    if (c >= 0xc2 && c <= 0xdf) { cp = c & 31; count = 1; minimum = 0x80; }
                    else if (c >= 0xe0 && c <= 0xef) { cp = c & 15; count = 2; minimum = 0x800; }
                    else if (c >= 0xf0 && c <= 0xf4) { cp = c & 7; count = 3; minimum = 0x10000; }
                    else Fail();
                    for (unsigned i = 0; i < count; ++i) {
                        if (Pos >= Input.size()) Fail();
                        const auto next = static_cast<unsigned char>(Input[Pos++]);
                        if ((next & 0xc0) != 0x80) Fail();
                        cp = cp * 64 + (next & 63);
                    }
                    if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) Fail();
                    Utf8(out, cp); continue;
                }
                if (Pos >= Input.size()) Fail();
                switch (Input[Pos++]) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned cp = Hex4();
                    if (cp >= 0xd800 && cp <= 0xdbff) {
                        Expect('\\'); Expect('u'); const unsigned low = Hex4();
                        if (low < 0xdc00 || low > 0xdfff) Fail();
                        cp = 0x10000 + ((cp - 0xd800) << 10) + low - 0xdc00;
                    } else if (cp >= 0xdc00 && cp <= 0xdfff) Fail();
                    Utf8(out, cp); break;
                }
                default: Fail();
                }
            }
            return out;
        }
        bool Digit() const { return Peek() >= '0' && Peek() <= '9'; }
        JSON Number() {
            const size_t begin = Pos; Take('-');
            if (!Take('0')) { if (!Digit()) Fail(); while (Digit()) ++Pos; }
            bool floating = false;
            if (Take('.')) { floating = true; if (!Digit()) Fail(); while (Digit()) ++Pos; }
            if (Take('e') || Take('E')) {
                floating = true; if (!Take('+')) Take('-');
                if (!Digit()) Fail(); while (Digit()) ++Pos;
            }
            const char* first = Input.data() + begin; const char* last = Input.data() + Pos;
            if (!floating) {
                long value; const auto r = std::from_chars(first, last, value);
                if (r.ec == std::errc{} && r.ptr == last) return JSON(value);
            }
            double value; const auto r = std::from_chars(first, last, value);
            if (r.ec != std::errc{} || r.ptr != last || !std::isfinite(value)) Fail();
            return JSON(value);
        }
        JSON Value(unsigned depth) {
            if (depth > 128) Fail(); WS();
            if (Peek() == '"') return JSON(String());
            if (Take('{')) {
                JSON object = Object(); WS(); if (Take('}')) return object;
                do {
                    WS(); const auto key = String(); WS(); Expect(':');
                    if (object.hasKey(key)) Fail();
                    object[key] = Value(depth + 1); WS();
                    if (Take('}')) return object;
                } while (Take(','));
                Fail();
            }
            if (Take('[')) {
                JSON array = Array(); WS(); if (Take(']')) return array;
                do {
                    array.append(Value(depth + 1)); WS();
                    if (Take(']')) return array;
                } while (Take(','));
                Fail();
            }
            for (const auto& token : {string("true"), string("false"), string("null")}) {
                if (Input.compare(Pos, token.size(), token) == 0) {
                    Pos += token.size();
                    return token == "null" ? JSON() : JSON(token == "true");
                }
            }
            if (Peek() == '-' || Digit()) return Number();
            Fail();
        }
    public:
        explicit Parser(const string& input) : Input(input) {}
        JSON Parse() { auto result = Value(0); WS(); if (Pos != Input.size()) Fail(); return result; }
    };
}

inline JSON JSON::Load(const string& str) { return Parser(str).Parse(); }
} // namespace json

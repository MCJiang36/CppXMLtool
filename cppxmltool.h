/**
 * @file cppxmltool.h
 *
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 MCJiang
 */

#ifndef MCJiang_CppXMLtool
#define MCJiang_CppXMLtool 0x0101 // V1.1

#ifdef _MSC_VER
#define CPP_VERSION _MSVC_LANG
#else
#define CPP_VERSION __cplusplus
#endif

#if CPP_VERSION < 201703L
#error "CppXMLtool: C++17 required"
#endif

#include <string> // for string
#include <vector> // for vector
#include <variant> // for variant
#include <unordered_map> // for unordered_map
#include <memory> // for unique_ptr
#include <utility> // for move
#include <stack> // for stack
#include <stdexcept> // for out_of_range
#include <cstddef> // for size_t
#include <cctype> // for isalpha, isalnum
#include <algorithm> // for sort

#undef CPP_VERSION

namespace mcjiang {

	/**
	 * @brief XML document
	 */
	struct xml {
		struct tag;

		typedef std::variant<std::string, std::unique_ptr<tag>> node; ///< AST node
		typedef std::vector<node> tree; ///< tree

		/**
		 * @brief XML tag type
		 */
		struct tag {
			std::string name; ///< tag name
			std::unordered_map<std::string, std::string> attrs; ///< attributes
			tree sub_tree; ///< sub-tree

			tag() = default; ///< default
			tag(const tag& other); ///< copy
			tag& operator=(const tag& other); ///< copy
			tag(tag&&) = default; ///< move
			tag& operator=(tag&&) = default; ///< move
			~tag() = default; ///< deconstructor
		};

	private:
		tag root_;

		[[noreturn]] static void fail(const char* msg, size_t pos) {
			throw std::runtime_error(std::string("[CppXMLtool] parse error: ") + msg
				+ " at offset " + std::to_string(pos));
		}

		static bool is_ws(char c) {
			return c == ' ' || c == '\t' || c == '\n' || c == '\r';
		}

		static void skip_ws(const std::string& s, size_t& pos) {
			while (pos < s.size() && is_ws(s[pos])) ++pos;
		}

		static bool starts_with(const std::string& s, size_t pos, const char* lit) {
			size_t n = std::char_traits<char>::length(lit);
			return s.compare(pos, n, lit) == 0;
		}

		static bool equals_ignore_case(const std::string& s, const char* lit) {
			size_t n = std::char_traits<char>::length(lit);
			if (s.size() != n) return false;
			for (size_t i = 0; i < n; ++i) {
				char a = s[i], b = lit[i];
				if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
				if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
				if (a != b) return false;
			}
			return true;
		}

		static unsigned long decode_utf8(const std::string& s, size_t& pos) {
			size_t start = pos;
			unsigned char c = static_cast<unsigned char>(s[pos]);
			if (c < 0x80) {
				++pos;
				return c;
			}

			size_t len;
			unsigned long cp;
			if (c >= 0xC2 && c <= 0xDF) { len = 2; cp = c & 0x1F; }
			else if (c >= 0xE0 && c <= 0xEF) { len = 3; cp = c & 0x0F; }
			else if (c >= 0xF0 && c <= 0xF4) { len = 4; cp = c & 0x07; }
			else fail("invalid UTF-8", start);

			if (pos + len > s.size()) fail("invalid UTF-8", start);
			for (size_t i = 1; i < len; ++i) {
				unsigned char cc = static_cast<unsigned char>(s[pos + i]);
				if ((cc & 0xC0) != 0x80) fail("invalid UTF-8", start);
				cp = (cp << 6) | (cc & 0x3F);
			}
			if ((len == 2 && cp < 0x80) || (len == 3 && cp < 0x800)
				|| (len == 4 && cp < 0x10000)
				|| (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF)
				fail("invalid UTF-8", start);

			pos += len;
			return cp;
		}

		static bool is_xml_char(unsigned long code) {
			return code == 0x09 || code == 0x0A || code == 0x0D
				|| (code >= 0x20 && code <= 0xD7FF)
				|| (code >= 0xE000 && code <= 0xFFFD)
				|| (code >= 0x10000 && code <= 0x10FFFF);
		}

		static bool is_name_start_cp(unsigned long c) {
			return c == ':' || (c >= 'A' && c <= 'Z') || c == '_' || (c >= 'a' && c <= 'z')
				|| (c >= 0xC0 && c <= 0xD6) || (c >= 0xD8 && c <= 0xF6)
				|| (c >= 0xF8 && c <= 0x2FF) || (c >= 0x370 && c <= 0x37D)
				|| (c >= 0x37F && c <= 0x1FFF) || (c >= 0x200C && c <= 0x200D)
				|| (c >= 0x2070 && c <= 0x218F) || (c >= 0x2C00 && c <= 0x2FEF)
				|| (c >= 0x3001 && c <= 0xD7FF) || (c >= 0xF900 && c <= 0xFDCF)
				|| (c >= 0xFDF0 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0xEFFFF);
		}

		static bool is_name_char_cp(unsigned long c) {
			return is_name_start_cp(c) || c == '-' || c == '.' || (c >= '0' && c <= '9')
				|| c == 0xB7 || (c >= 0x300 && c <= 0x36F) || (c >= 0x203F && c <= 0x2040);
		}

		static std::string parse_name(const std::string& s, size_t& pos) {
			size_t start = pos;
			if (pos >= s.size()) fail("expected name", pos);
			unsigned long cp = decode_utf8(s, pos);
			if (!is_name_start_cp(cp)) fail("expected name", start);

			for (;;) {
				if (pos >= s.size()) break;
				size_t save = pos;
				cp = decode_utf8(s, pos);
				if (!is_name_char_cp(cp)) {
					pos = save;
					break;
				}
			}
			return s.substr(start, pos - start);
		}

		static void skip_bom(const std::string& s, size_t& pos) {
			if (s.size() >= 3
				&& static_cast<unsigned char>(s[0]) == 0xEF
				&& static_cast<unsigned char>(s[1]) == 0xBB
				&& static_cast<unsigned char>(s[2]) == 0xBF)
				pos = 3;
		}

		static void append_utf8(unsigned long code, std::string& out) {
			if (code <= 0x7F) {
				out.push_back(static_cast<char>(code));
			}
			else if (code <= 0x7FF) {
				out.push_back(static_cast<char>(0xC0 | (code >> 6)));
				out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
			}
			else if (code <= 0xFFFF) {
				out.push_back(static_cast<char>(0xE0 | (code >> 12)));
				out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
				out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
			}
			else {
				out.push_back(static_cast<char>(0xF0 | (code >> 18)));
				out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
				out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
				out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
			}
		}

		static void decode_entity(const std::string& s, size_t& pos, std::string& out) {
			++pos;
			if (pos < s.size() && s[pos] == '#') {
				++pos;
				unsigned long base = 10;
				if (pos < s.size() && s[pos] == 'x') {
					base = 16;
					++pos;
				}
				size_t start = pos;
				while (pos < s.size() && s[pos] != ';') ++pos;
				if (pos >= s.size()) fail("unterminated entity", pos);
				std::string num = s.substr(start, pos - start);
				++pos;
				if (num.empty()) fail("empty entity", pos);

				unsigned long code = 0;
				for (char c : num) {
					unsigned long digit;
					if (c >= '0' && c <= '9') digit = static_cast<unsigned long>(c - '0');
					else if (base == 16 && c >= 'a' && c <= 'f') digit = static_cast<unsigned long>(c - 'a' + 10);
					else if (base == 16 && c >= 'A' && c <= 'F') digit = static_cast<unsigned long>(c - 'A' + 10);
					else fail("invalid numeric entity", pos);
					if (digit >= base) fail("invalid numeric entity", pos);
					code = code * base + digit;
					if (code > 0x10FFFF) fail("entity out of range", pos);
				}
				if (!is_xml_char(code)) fail("invalid character reference", pos);
				append_utf8(code, out);
				return;
			}
			size_t start = pos;
			while (pos < s.size() && s[pos] != ';') ++pos;
			if (pos >= s.size()) fail("unterminated entity", pos);
			std::string name = s.substr(start, pos - start);
			++pos;
			if (name == "amp") out.push_back('&');
			else if (name == "lt") out.push_back('<');
			else if (name == "gt") out.push_back('>');
			else if (name == "quot") out.push_back('"');
			else if (name == "apos") out.push_back('\'');
			else fail("unknown entity", pos);
		}

		static std::string parse_text(const std::string& s, size_t& pos) {
			std::string out;
			while (pos < s.size() && s[pos] != '<') {
				char c = s[pos++];
				if (c == ']' && pos + 1 < s.size() && s[pos] == ']' && s[pos + 1] == '>')
					fail("']]>' is not allowed in character data", pos);
				if (c == '&') {
					--pos;
					decode_entity(s, pos, out);
				}
				else {
					out.push_back(c);
				}
			}
			return out;
		}

		static std::string parse_attr_value(const std::string& s, size_t& pos) {
			if (pos >= s.size() || (s[pos] != '"' && s[pos] != '\''))
				fail("expected quote", pos);
			char quote = s[pos++];
			std::string out;
			while (pos < s.size() && s[pos] != quote) {
				char c = s[pos];
				if (c == '<') fail("'<' is not allowed in attribute value", pos);
				if (c == '&') {
					decode_entity(s, pos, out);
					continue;
				}
				if (c == '\t' || c == '\n' || c == '\r') out.push_back(' ');
				else out.push_back(c);
				++pos;
			}
			if (pos >= s.size()) fail("unterminated attribute value", pos);
			++pos;
			return out;
		}

		static void skip_comment(const std::string& s, size_t& pos) {
			size_t start = pos;
			pos += 4;
			while (pos < s.size() && !starts_with(s, pos, "-->")) {
				if (s[pos] == '-' && pos + 1 < s.size() && s[pos + 1] == '-')
					fail("'--' is not allowed inside a comment", pos);
				++pos;
			}
			if (pos >= s.size()) fail("unterminated comment", start);
			pos += 3;
		}

		static void parse_cdata(const std::string& s, size_t& pos, std::string& out) {
			size_t start = pos;
			pos += 9;
			size_t begin = pos;
			while (pos < s.size() && !starts_with(s, pos, "]]>")) ++pos;
			if (pos >= s.size()) fail("unterminated CDATA", start);
			out.append(s, begin, pos - begin);
			pos += 3;
		}

		static void skip_pi(const std::string& s, size_t& pos) {
			size_t start = pos;
			pos += 2;
			if (pos >= s.size()) fail("unexpected EOF in processing instruction", pos);
			std::string target = parse_name(s, pos);
			if (equals_ignore_case(target, "xml"))
				fail("reserved processing instruction target 'xml'", start);
			if (starts_with(s, pos, "?>")) {
				pos += 2;
				return;
			}
			if (pos >= s.size() || !is_ws(s[pos]))
				fail("expected whitespace after processing instruction target", pos);
			while (pos < s.size() && !starts_with(s, pos, "?>")) ++pos;
			if (pos >= s.size()) fail("unterminated processing instruction", start);
			pos += 2;
		}

		static std::string normalize_newlines(const std::string& s) {
			std::string out;
			out.reserve(s.size());
			for (size_t i = 0; i < s.size(); ++i) {
				if (s[i] == '\r') {
					out.push_back('\n');
					if (i + 1 < s.size() && s[i + 1] == '\n') ++i;
				}
				else {
					out.push_back(s[i]);
				}
			}
			return out;
		}

		static void validate_chars(const std::string& s) {
			size_t pos = 0;
			while (pos < s.size()) {
				size_t start = pos;
				unsigned long cp = decode_utf8(s, pos);
				if (!is_xml_char(cp)) fail("invalid character", start);
			}
		}

		static void parse_decl(const std::string& s, size_t& pos) {
			size_t start = pos;
			pos += 5;
			if (pos >= s.size() || !is_ws(s[pos]))
				fail("expected whitespace after '<?xml'", pos);
			skip_ws(s, pos);

			std::string key = parse_name(s, pos);
			if (key != "version") fail("expected 'version' in XML declaration", pos);
			skip_ws(s, pos);
			if (pos >= s.size() || s[pos] != '=') fail("expected '=' in XML declaration", pos);
			++pos;
			skip_ws(s, pos);
			std::string version = parse_attr_value(s, pos);
			if (version != "1.0") fail("unsupported XML version (only 1.0)", pos);

			bool saw_encoding = false;
			bool saw_standalone = false;
			for (;;) {
				size_t before = pos;
				skip_ws(s, pos);
				bool had_ws = pos > before;
				if (starts_with(s, pos, "?>")) {
					pos += 2;
					return;
				}
				if (pos >= s.size()) fail("unterminated XML declaration", start);
				if (!had_ws) fail("expected whitespace in XML declaration", pos);

				std::string field = parse_name(s, pos);
				if (field == "encoding") {
					if (saw_encoding || saw_standalone)
						fail("unexpected 'encoding' in XML declaration", pos);
					saw_encoding = true;
					skip_ws(s, pos);
					if (pos >= s.size() || s[pos] != '=') fail("expected '=' in XML declaration", pos);
					++pos;
					skip_ws(s, pos);
					std::string enc = parse_attr_value(s, pos);
					if (enc.empty() || !std::isalpha(static_cast<unsigned char>(enc[0]))
						|| !std::isalnum(static_cast<unsigned char>(enc[0])))
						fail("invalid encoding name", pos);
					for (char c : enc)
						if (!std::isalnum(static_cast<unsigned char>(c))
							&& c != '.' && c != '_' && c != '-')
							fail("invalid encoding name", pos);
				}
				else if (field == "standalone") {
					if (saw_standalone) fail("duplicate 'standalone' in XML declaration", pos);
					saw_standalone = true;
					skip_ws(s, pos);
					if (pos >= s.size() || s[pos] != '=') fail("expected '=' in XML declaration", pos);
					++pos;
					skip_ws(s, pos);
					std::string value = parse_attr_value(s, pos);
					if (value != "yes" && value != "no")
						fail("standalone must be 'yes' or 'no'", pos);
				}
				else {
					fail("unexpected field in XML declaration", pos);
				}
			}
		}

		static void push_text(tag& t, std::string text) {
			if (text.empty()) return;
			if (!t.sub_tree.empty()) {
				if (auto* last = std::get_if<std::string>(&t.sub_tree.back())) {
					last->append(text);
					return;
				}
			}
			t.sub_tree.push_back(std::move(text));
		}

		static tag parse_tag(const std::string& s, size_t& pos) {
			if (pos >= s.size() || s[pos] != '<') fail("expected '<'", pos);
			++pos;
			tag t;
			t.name = parse_name(s, pos);

			for (;;) {
				size_t before = pos;
				skip_ws(s, pos);
				bool had_ws = pos > before;

				if (pos < s.size() && s[pos] == '/') {
					++pos;
					if (pos >= s.size() || s[pos] != '>')
						fail("expected '>' after '/'", pos);
					++pos;
					return t;
				}
				if (pos < s.size() && s[pos] == '>') {
					++pos;
					break;
				}
				if (pos >= s.size()) fail("unexpected EOF in tag", pos);
				if (!had_ws) fail("expected whitespace before attribute", pos);

				std::string key = parse_name(s, pos);
				skip_ws(s, pos);
				if (pos >= s.size() || s[pos] != '=')
					fail("expected '=' after attribute name", pos);
				++pos;
				skip_ws(s, pos);
				std::string val = parse_attr_value(s, pos);
				if (t.attrs.find(key) != t.attrs.end())
					fail("duplicate attribute", pos);
				t.attrs.emplace(std::move(key), std::move(val));
			}

			for (;;) {
				if (pos >= s.size()) fail("unexpected EOF in content", pos);
				if (starts_with(s, pos, "</")) {
					pos += 2;
					std::string close_name = parse_name(s, pos);
					if (close_name != t.name) fail("mismatched closing tag", pos);
					skip_ws(s, pos);
					if (pos >= s.size() || s[pos] != '>')
						fail("expected '>' in closing tag", pos);
					++pos;
					break;
				}
				if (starts_with(s, pos, "<!--")) {
					skip_comment(s, pos);
					continue;
				}
				if (starts_with(s, pos, "<![CDATA[")) {
					std::string cdata;
					parse_cdata(s, pos, cdata);
					push_text(t, std::move(cdata));
					continue;
				}
				if (starts_with(s, pos, "<?")) {
					skip_pi(s, pos);
					continue;
				}
				if (starts_with(s, pos, "<!")) {
					if (starts_with(s, pos, "<!DOCTYPE"))
						fail("DTD is not supported", pos);
					fail("unexpected markup declaration", pos);
				}
				if (s[pos] == '<') {
					tag child = parse_tag(s, pos);
					t.sub_tree.push_back(std::make_unique<tag>(std::move(child)));
					continue;
				}
				std::string text = parse_text(s, pos);
				push_text(t, std::move(text));
			}
			return t;
		}

		static void skip_misc(const std::string& s, size_t& pos) {
			for (;;) {
				skip_ws(s, pos);
				if (starts_with(s, pos, "<?")) { skip_pi(s, pos); continue; }
				if (starts_with(s, pos, "<!--")) { skip_comment(s, pos); continue; }
				break;
			}
		}

		static tag parse_doc(const std::string& input) {
			std::string s = normalize_newlines(input);
			validate_chars(s);

			size_t pos = 0;
			skip_bom(s, pos);

			if (starts_with(s, pos, "<?xml")
				&& (pos + 5 >= s.size() || is_ws(s[pos + 5]) || s[pos + 5] == '?'))
				parse_decl(s, pos);

			skip_misc(s, pos);

			if (starts_with(s, pos, "<!DOCTYPE"))
				fail("DTD is not supported", pos);
			if (pos >= s.size()) fail("empty document", pos);
			if (starts_with(s, pos, "<!"))
				fail("unexpected markup declaration before root element", pos);

			tag root = parse_tag(s, pos);

			skip_misc(s, pos);
			if (pos < s.size()) fail("trailing content after root element", pos);
			return root;
		}

		static tag* find_first(tag* root, const std::string& key) {
			std::stack<tag*> st;
			st.push(root);
			while (!st.empty()) {
				tag* t = st.top();
				st.pop();
				if (t->name == key) return t;
				for (auto it = t->sub_tree.rbegin(); it != t->sub_tree.rend(); ++it)
					if (auto* u = std::get_if<std::unique_ptr<tag>>(&*it))
						st.push(u->get());
			}
			return nullptr;
		}

	public:
		xml() = default; ///< default constructor

		xml(const std::string& text) ///< parse XML document
		{
			root_ = parse_doc(text);
		}

		xml(const tag& new_root) : root_(new_root) {} ///< copy from tag
		xml(tag&& new_root) noexcept : root_(std::move(new_root)) {} ///< move from tag

		xml(const xml&) = default; ///< copy constructor
		xml(xml&&) = default; ///< move constructor

		~xml() = default; ///< deconstructor

		xml& operator=(const xml&) = default; ///< copy assignment
		xml& operator=(xml&&) = default; ///< move assignment

		tag& root() noexcept { return root_; } ///< get root
		const tag& root() const noexcept { return root_; } ///< get root

		tag& root(const tag& new_root) ///< set root (copy)
		{
			*this = xml(new_root);
			return root_;
		}
		tag& root(tag&& new_root) ///< set root (move)
		{
			*this = xml(std::move(new_root));
			return root_;
		}

		/**
		 * @brief parse XML and replace current tree
		 *
		 * @param text XML text
		 */
		void load(const std::string& text) {
			root_ = parse_doc(text);
		}

		/**
		 * @brief convert to string
		 */
		std::string to_str() const {
			if (root_.name.empty()) return std::string();

			std::string res;
			res.reserve(512);
			res += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";

			auto escape_attr = [](const std::string& v, std::string& out) {
				for (char c : v) {
					switch (c) {
					case '&':  out += "&amp;";  break;
					case '<':  out += "&lt;";   break;
					case '>':  out += "&gt;";   break;
					case '\"': out += "&quot;"; break;
					case '\'': out += "&apos;"; break;
					case 0x09: out += "&#x09;"; break;
					case 0x0A: out += "&#x0A;"; break;
					case 0x0D: out += "&#x0D;"; break;
					default:   out.push_back(c); break;
					}
				}
				};

			auto recurse = [&res, &escape_attr](const tag& t, auto& self) -> void {
				res += "<";
				res += t.name;
				std::vector<const std::pair<const std::string, std::string>*> sorted;
				sorted.reserve(t.attrs.size());
				for (const auto& kv : t.attrs) sorted.push_back(&kv);
				if (sorted.size() > 1)
					std::sort(sorted.begin(), sorted.end(),
						[](const auto* a, const auto* b) { return a->first < b->first; });

				for (const auto* kv : sorted) {
					res += " ";
					res += kv->first;
					res += "=\"";
					escape_attr(kv->second, res);
					res += "\"";
				}
				res += ">";

				for (const auto& n : t.sub_tree) {
					if (auto* s = std::get_if<std::string>(&n)) {
						for (char c : *s) {
							switch (c) {
							case '&':  res += "&amp;"; break;
							case '<':  res += "&lt;";  break;
							case '>':  res += "&gt;";  break;
							case 0x0D: res += "&#x0D;"; break;
							default:   res.push_back(c); break;
							}
						}
					}
					else if (auto* u = std::get_if<std::unique_ptr<tag>>(&n)) {
						self(**u, self);
					}
				}

				res += "</";
				res += t.name;
				res += ">";
				};

			recurse(root_, recurse);
			return res;
		}

		/**
		 * @brief XML range
		 */
		class range {
			tag* tag_;
			bool is_local;

		public:
			explicit range(const tag& init) : tag_(new tag(init)), is_local(true) {} ///< constructor (owns a copy)
			explicit range(tag* init) : tag_(init), is_local(false) {} ///< constructor (borrows)

			range(const range&) = delete; ///< non-copyable
			range& operator=(const range&) = delete; ///< non-copyable

			range(range&& other) noexcept : tag_(other.tag_), is_local(other.is_local) ///< move
			{
				other.tag_ = nullptr;
				other.is_local = false;
			}

			range& operator=(range&& other) noexcept ///< move
			{
				if (this != &other) {
					if (is_local) delete tag_;
					tag_ = other.tag_;
					is_local = other.is_local;
					other.tag_ = nullptr;
					other.is_local = false;
				}
				return *this;
			}

			~range() {
				if (is_local) delete tag_;
			}

			tree::iterator begin() { return tag_->sub_tree.begin(); } ///< begin
			tree::iterator end() { return tag_->sub_tree.end(); } ///< end

			tree::const_iterator cbegin() const { return tag_->sub_tree.cbegin(); } ///< constant begin
			tree::const_iterator cend() const { return tag_->sub_tree.cend(); } ///< constant end
		};

		/**
		 * @brief XML recursing range
		 *
		 * it will iterate the entire XML tree
		 */
		class recurse_range {
			template<class Ty, class It, class Rf>
			class basic_iterator {
				std::stack<Ty*> container;
				std::stack<It> curs;
				bool ended;

				void normalize() {
					while (!curs.empty()) {
						if (curs.top() != container.top()->sub_tree.end()) return;
						container.pop();
						curs.pop();
						if (!curs.empty()) ++curs.top();
					}
					ended = true;
				}

			public:
				basic_iterator() : container(), curs(), ended(true) {}

				explicit basic_iterator(Ty* tag_) : container(), curs(), ended(false) {
					if (!tag_) {
						ended = true;
						return;
					}
					container.push(tag_);
					curs.push(tag_->sub_tree.begin());
					normalize();
				}

				basic_iterator(const basic_iterator&) = default;
				basic_iterator(basic_iterator&&) = default;
				basic_iterator& operator=(const basic_iterator&) = default;
				basic_iterator& operator=(basic_iterator&&) = default;
				~basic_iterator() = default;

				Rf operator*() {
					return *curs.top();
				}

				basic_iterator& operator++() {
					if (ended) return *this;

					if (auto* u = std::get_if<std::unique_ptr<tag>>(&(*curs.top()))) {
						container.push(u->get());
						curs.push(u->get()->sub_tree.begin());
					}
					else {
						++curs.top();
					}
					normalize();
					return *this;
				}

				basic_iterator operator++(int) {
					basic_iterator tmp(*this);
					++* this;
					return tmp;
				}

				bool operator==(const basic_iterator& other) const {
					if (ended && other.ended) return true;
					if (ended || other.ended) return false;
					return curs.top() == other.curs.top();
				}

				bool operator!=(const basic_iterator& other) const {
					return !(*this == other);
				}
			};

		public:
			typedef basic_iterator<tag, tree::iterator, node&> iterator;
			typedef basic_iterator<const tag, tree::const_iterator, const node&> const_iterator;

		private:
			tag* root_;
			bool is_local;

		public:
			explicit recurse_range(const tag& init) : root_(new tag(init)), is_local(true) {} ///< constructor (owns a copy)
			explicit recurse_range(tag* init) : root_(init), is_local(false) {} ///< constructor (borrows)

			recurse_range(const recurse_range&) = delete; ///< non-copyable
			recurse_range& operator=(const recurse_range&) = delete; ///< non-copyable

			recurse_range(recurse_range&& other) noexcept ///< move
				: root_(other.root_), is_local(other.is_local) {
				other.root_ = nullptr;
				other.is_local = false;
			}

			recurse_range& operator=(recurse_range&& other) noexcept ///< move
			{
				if (this != &other) {
					if (is_local) delete root_;
					root_ = other.root_;
					is_local = other.is_local;
					other.root_ = nullptr;
					other.is_local = false;
				}
				return *this;
			}

			~recurse_range() ///< deconstructor
			{
				if (is_local) delete root_;
			}

			iterator begin() { return iterator(root_); } ///< begin
			iterator end() { return iterator(); } ///< end

			const_iterator cbegin() const { return const_iterator(root_); } ///< constant begin
			const_iterator cend() const { return const_iterator(); } ///< constant end
		};

		tag* find(const std::string& key) {
			return find_first(&root_, key);
		}

		const tag* find(const std::string& key) const {
			return find_first(const_cast<tag*>(&root_), key);
		}

		static bool is_text(const node& node_) ///< is text
		{
			return std::holds_alternative<std::string>(node_);
		}

		static bool is_tag(const node& node_) ///< is XML tag
		{
			return std::holds_alternative<std::unique_ptr<tag>>(node_);
		}

		size_t tag_count() const ///< tag count
		{
			size_t res = 1;
			std::stack<const tag*> st;
			st.push(&root_);
			while (!st.empty()) {
				const tag* t = st.top();
				st.pop();
				for (const auto& n : t->sub_tree) {
					if (auto* u = std::get_if<std::unique_ptr<tag>>(&n)) {
						++res;
						st.push(u->get());
					}
				}
			}
			return res;
		}
	};

	/**
		 * @brief deep-copy constructor
		 * @param other source tag
		 */
	xml::tag::tag(const tag& other)
		: name(other.name)
		, attrs(other.attrs) {
		sub_tree.reserve(other.sub_tree.size());
		for (const auto& n : other.sub_tree) {
			if (std::holds_alternative<std::string>(n))
				sub_tree.push_back(std::get<std::string>(n));
			else
				sub_tree.push_back(
					std::make_unique<tag>(*std::get<std::unique_ptr<tag>>(n)));
		}
	}

	/**
	 * @brief deep-copy assignment
	 * @param other source tag
	 * @return *this
	 */
	xml::tag& xml::tag::operator=(const tag& other) {
		if (this != &other) {
			tag tmp(other);
			name = std::move(tmp.name);
			attrs = std::move(tmp.attrs);
			sub_tree = std::move(tmp.sub_tree);
		}
		return *this;
	}

} // namespace mcjiang

#endif // MCJiang_CppXMLtool

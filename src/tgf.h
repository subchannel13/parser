// LICENSE
// This software is free for use and redistribution while including this
// license notice, unless:
// 1. is used for commercial or non-personal purposes, or
// 2. used for a product which includes or associated with a blockchain or other
// decentralized database technology, or
// 3. used for a product which includes or associated with the issuance or use
// of cryptographic or electronic currencies/coins/tokens.
// On all of the mentioned cases, an explicit and written permission is required
// from the Author (Ohad Asor).
// Contact ohad@idni.org for requesting a permission. This license may be
// modified over time by the Author.
#ifndef __IDNI__PARSER__TGF_H__
#define __IDNI__PARSER__TGF_H__
#include <fstream>
#include <streambuf>

#include "parser_instance.h"
#include "tgf_parser.generated.h"
#include "traverser.h"
#include "devhelpers.h"

namespace idni {

static inline std::ostream& print_node(std::ostream& os,
	const idni::rewriter::sp_node<std::variant<tgf_parser::symbol_type>>& n,
	size_t indent = 0)
{
	for (size_t i = 0; i != indent; ++i) os << "\t";
	if (std::holds_alternative<tgf_parser::symbol_type>(n->value)) {
		auto& l = std::get<tgf_parser::symbol_type>(n->value);
		if (l.nt()) os << "`" << l << "`";
		else if (!l.is_null()) os << l;
	}
	os << "\n";
	for (const auto& c : n->child) print_node(os, c, indent + 1);
	return os;
}

template <typename C = char, typename T = C>
struct tgf {
	using symbol_t       = tgf_parser::symbol_type;
	using node_variant_t = std::variant<symbol_t>;
	using node_t         = idni::rewriter::sp_node<node_variant_t>;
	using traverser_t    = traverser<node_variant_t, tgf_parser>;
	using lit_t          = lit<C, T>;
	using prods_t        = prods<C, T>;

	static grammar<C, T> from_string(nonterminals<C, T>& nts_,
		const std::basic_string<C>& s)
	{
		grammar_builder b(nts_);
		auto c = s.c_str();
		size_t last = 0;
		auto l = s.size();
		//std::cout << "parsing: " << to_std_string(s) << std::endl;
		bool in_string = false;
		//DBG(f.g.print_data(std::cout << "\n>>>\n\n") << "\n<<<" << std::endl;)
		bool in_comment = false;
		bool in_char = false;
		bool in_escape = false;

		auto escape = [&in_escape](char c) {
			bool ret = in_escape;
			if (in_escape) in_escape = false;
			if (ret) return ret;
			return in_escape = (c == '\\');
		};
		for (size_t i = 0; i != l; ++i) {
			//std::cout << "i: " << i << " c[i]: '" << c[i]
			//	<< "' string: " << in_string
			//	<< " comment: " << in_comment
			//	<< " char: " << in_char
			//	<< " escape: " << in_escape << std::endl;
			if (in_comment) {
				if (c[i] == '\n') in_comment = false;
			}
			else if (in_string) {
				if (escape(c[i])) continue;
				if (c[i] == '"') in_string = false;
			}
			else if (in_char) {
				if (escape(c[i])) continue;
				if (c[i] == '\'') in_char = false;
			}
			else if (c[i] == '\'') in_char = true;
			else if (c[i] == '"') in_string = true;
			else if (c[i] == '#') in_comment = true;
			else if (c[i] == '.') {
				const char* nc = c + last;
				//std::cout << "last: " << last << " " << c[last] << std::endl;
				//std::cout << "i:    " << i << " " << c[i] << std::endl;
				size_t nl = i - last + 1;
				//std::cout << "nl:   " << nl << std::endl;
				//std::cout << "nc:   {";
				//for (size_t j = 0; j < nl; ++j) {
				//	std::cout << nc[j];
				//}
				//std::cout << "}\n";
				last = i + 1;
				//std::cout << "parsing: " << to_std_string(std::basic_string<C>(nc, nl)) << std::endl;
				int ret = b.parse(nc, nl);
				//std::cout << "ret:   " << ret << std::endl;
				if (ret == 1) break;
			}
		}
		return b.g();
	}

	static grammar<C, T> from_file(nonterminals<C, T>& nts_,
		const std::string& filename)
	{
		std::ifstream ifs(filename);
		if (!ifs) {
			std::cerr << "cannot open file: " << filename << std::endl;
			return grammar<C, T>(nts_);
		}
		return from_string(nts_, std::string(
				std::istreambuf_iterator<C>(ifs),
				std::istreambuf_iterator<C>()));
	}

	static grammar<C, T> from_forest(nonterminals<C, T>& nts_,
		std::unique_ptr<typename parser<C, T>::pforest> f,
		const std::basic_string<C>& start_nt = from_cstr<C>("start"))
	{
		auto& p = parser_instance<tgf_parser>();
		if (!p.found(p.id(start_nt))) return std::cout << "TGF: "
			<< p.get_error().to_str() << "\n", grammar<C, T>(nts_);
		if (!f) return std::cout << "No parse forest\n",
							grammar<C, T>(nts_);
		auto n_trees = f->count_trees();
		if (f->is_ambiguous() && n_trees > 1) std::cout
			<< "\nambiguity... number of trees: " << n_trees <<"\n";
		char dummy = '\0'; // as a dummy transformer
		auto source = idni::rewriter::make_node_from_forest<
			tgf_parser, char, tgf_parser::node_type,
			node_variant_t>(dummy, f.get());
		//print_node(std::cout << "source: `", source) << "`\n";
		traverser_t t(source);
		grammar_builder b(nts_, t);
		return b.g();
	}

private:
	struct grammar_builder {
		static constexpr const auto& get_only_child =
			traverser_t::get_only_child_extractor();
		static constexpr const auto& get_terminals =
			traverser_t::get_terminal_extractor();
		static constexpr const auto& get_nonterminal =
			traverser_t::get_nonterminal_extractor();
		prods_t ps, nul{ lit_t{} };
		nonterminals<C, T>& nts;
		// set by @start ...
		prods_t start = nul;
		// char class names coming from @use char class ...
		std::vector<std::string> cc_names{};
		// grammar options
		grammar<C, T>::options opt{};
		char_class_fns<T> cc;
		grammar_builder(nonterminals<C, T>& nts) : nts(nts) {}
		grammar_builder(nonterminals<C, T>& nts, const traverser_t& t)
			: nts(nts) { build(t); }
		void build(const traverser_t& t) {
			auto statements  = t || tgf_parser::statement;
			auto directives  = statements || tgf_parser::directive;
			for (const auto& d : directives()) directive(d);
			cc = predefined_char_classes<C, T>(cc_names, nts);
			auto productions = statements || tgf_parser::production;
			for (const auto& pr : productions()) production(pr);
		}
		int parse(const char* s, size_t l) {
			auto& p = parser_instance<tgf_parser>();
			static tgf_parser::parse_options po{
				.start = tgf_parser::start_statement };
			auto f = p.parse(s, l, po);
			if (!p.found(po.start) || !f) return std::cerr
				<< "TGF: " << p.get_error().to_str() << "\n", 1;
			char dummy = '\0';
			auto source = idni::rewriter::make_node_from_forest<
				tgf_parser, char, tgf_parser::node_type,
				node_variant_t>(dummy, f.get());
			//print_node(std::cout << "source: `", source) << "`\n";
			build(traverser_t(source));
			return 0;
		}
		grammar<C, T> g() {
			//std::cout << "opt.trim_terminals: " << opt.trim_terminals << "\n";
			//std::cout << "opt.inline_char_classes: " << opt.inline_char_classes << "\n";
			//std::cout << "opt.to_trim: ";
			//for (auto& n : opt.to_trim) std::cout << n << " ";
			//std::cout << "\n";
			//std::cout << "opt.to_trim_children: ";
			//for (auto& n : opt.to_trim_children) std::cout << n << " ";
			//std::cout << "\n";
			//std::cout << "opt.to_inline: ";
			//for (auto& n : opt.to_inline) std::cout << n << " ";
			//std::cout << "\n";
			return grammar<C, T>(nts, ps, start == nul
				? prods_t(nts("start")) : start, cc, opt);
		}
	private:
		size_t id = 0;
		void directive(const traverser_t& t) {
			auto node2nt = [this](const traverser_t& t) -> size_t {
				return nts.get(t | get_terminals);
			};
			//print_node(std::cout << "directive: ", t.value()) << "\n";
			auto d = t | tgf_parser::directive_body | get_only_child;
			auto nt = d | get_nonterminal;
			//print_node(std::cout << "nt: " << nt << " directive_body: ", d.value()) << "\n";
			switch (nt) {
			case tgf_parser::use_dir:
				for (auto& cc : (d
					|| tgf_parser::use_param
					|| tgf_parser::char_class_name)())
				{
					auto s = cc | get_terminals;
					//std::cout << "use char class: `" << s << "`\n";
					cc_names.push_back(s);
				}
				break;
			case tgf_parser::start_dir:
				start = prods_t(node2nt(d | tgf_parser::sym));
				break;
			case tgf_parser::trim_terminals_dir:
				opt.trim_terminals = true;
				break;
			case tgf_parser::trim_dir:
				for (auto& n : (d || tgf_parser::sym)())
					opt.to_trim.insert(node2nt(n));
				break;
			case tgf_parser::trim_children_dir:
				for (auto& n : (d || tgf_parser::sym)())
					opt.to_trim_children.insert(node2nt(n));
				break;
			case tgf_parser::inline_dir:
				for (auto& n : (d || tgf_parser::inline_arg)())
					if ((n | get_only_child
						| get_nonterminal) ==
						tgf_parser::char_classes_sym)
						opt.inline_char_classes = true;
					else opt.to_inline.insert(node2nt(n));
				break;
			case tgf_parser::boolean_dir: {
				auto action = d | tgf_parser::boolean_action;
				opt.auto_disambiguate = !action.has_value()
					|| (action | get_only_child
						| get_nonterminal) ==
							tgf_parser::enable_sym;
				break;
			}
			case tgf_parser::nodisambig_dir:
				for (auto& n : (d || tgf_parser::sym)())
					opt.nodisambig_list.insert(node2nt(n));
				break;
			default: return;
			}
		}
		void production(const traverser_t& t) {
			//print_node(std::cout, t.value()) << "\n";
			prods_t sym(nts(t | tgf_parser::sym | get_terminals));
			//std::cout << "sym: " << sym << std::endl;
			alternation(sym, t | tgf_parser::alternation);
		}
		void alternation(const prods_t& sym, const traverser_t& t) {
			//print_node(std::cout << "alternation: ", t.value()) << "\n";
			for (auto& c : (t || tgf_parser::conjunction)())
				ps(sym, conjunction(sym, c));
		}
		prods_t optional(const prods_t& sym, const prods_t& t)
		{
			auto nn = get_new_name(sym);
			return ps(nn, t), ps(nn, nul), nn;
		}
		prods_t repeat(const prods_t& sym, const prods_t& t) {
			auto nn = get_new_name(sym);
			return ps(nn, t | (t + nn)), nn;
		}
		prods_t none_or_repeat(const prods_t& sym, const prods_t& t) {
			auto nn = get_new_name(sym);
			return ps(nn, (t + nn) | nul), nn;
		}
		prods_t group(const prods_t& sym, const traverser_t& t) {
			auto nn = get_new_name(sym);
			alternation(nn.to_lit(), t | tgf_parser::alternation);
			return nn;
		}
		prods_t optional_group(const prods_t& sym, const traverser_t& t)
		{
			auto nn = get_new_name(sym);
			alternation(nn.to_lit(), t | tgf_parser::alternation);
			return ps(nn, nul), nn;
		}
		prods_t repeat_group(const prods_t& sym, const traverser_t& t) {
			auto nn = get_new_name(sym);
			auto nr = get_new_name(sym);
			alternation(nr.to_lit(), t | tgf_parser::alternation);
			return ps(nn, nr | (nr + nn)), nn;
		}
		std::basic_string<T> unescape(const std::string& s) {
			//std::cout << "unescape: `" << s << "`\n";
			std::basic_stringstream<T> ss;
			size_t l = s.size() - 1;
			for (size_t i = 0; i != s.size(); ++i) {
				if (i != l && s[i] == '\\') {
					switch (s[++i]) {
					case 'r': ss << '\r'; break;
					case 'n': ss << '\n'; break;
					case 't': ss << '\t'; break;
					case 'b': ss << '\b'; break;
					case 'f': ss << '\f'; break;
					default: ss << s[i];
					}
				} else ss << s[i];
			}
			//std::cout << "\t: `" << ss.str() << "`\n";
			return ss.str();
		}
		prods_t terminal_char(const traverser_t& t) {
			//print_node(std::cout << "terminal_char: ", t.value()) << "\n";
			auto c = t | tgf_parser::unescaped_c;
			if (c.has_value()) {
				//std::cout << "unescaped_c: `" << (c | get_terminals) << "`\n";
				return prods_t(c | get_terminals);
			}
			c = t | tgf_parser::escaped_c;
			return prods_t(unescape(c | get_terminals));
		}
		prods_t terminal_string(const traverser_t& t) {
			prods_t r{};
			auto cs = t || tgf_parser::terminal_string_char;
			for (auto& ch : cs()) {
				auto c = ch | tgf_parser::unescaped_s;
				if (c.has_value()) r = r + prods_t(c | get_terminals);
				else {
					c = ch | tgf_parser::escaped_s;
					r = r + prods_t(unescape(c | get_terminals));
				}
			}
			return r;
		}
		prods_t terminal(const traverser_t& t) {
			//print_node(std::cout << "terminal: ", t.value()) << "\n";
			auto c = t | get_only_child;
			if ((c | get_nonterminal) == tgf_parser::terminal_char)
				return terminal_char(c);
			else return terminal_string(c);
		}
		prods_t term(const prods_t& sym, const traverser_t& t) {
			//print_node(std::cout << "term: ", t.value()) << "\n";
			auto x = t | get_only_child;
			//std::cout << "term: " << (x | get_nonterminal) << std::endl;
			switch (x | get_nonterminal) {
			case tgf_parser::sym: {
				auto s = x | get_terminals;
				if (s == "null") return nul;
				return prods_t(nts(s));
			}
			case tgf_parser::terminal:       return terminal(x);
			case tgf_parser::group:          return group(sym, x);
			case tgf_parser::optional_group: return optional_group(sym, x);
			case tgf_parser::repeat_group:   return repeat_group(sym, x);
			default: assert(false);
			}
			return {};
		}
		prods_t factor(const prods_t& sym, const traverser_t& t) {
			//print_node(std::cout << "factor: ", t.value()) << "\n";
			auto f = t | get_only_child;
			auto nt = f | get_nonterminal;
			if (nt == tgf_parser::term) return term(sym, f);
			auto trm = term(sym, f | tgf_parser::term);
			switch (nt) {
			case tgf_parser::optional:       return optional(sym, trm);
			case tgf_parser::repeat:         return repeat(sym, trm);
			case tgf_parser::none_or_repeat: return none_or_repeat(sym, trm);
			case tgf_parser::neg:            return ~trm;
			default: assert(false);
			}
			return {};
		}
		prods_t concatenation(const prods_t& sym, const traverser_t& t){
			//print_node(std::cout << "concatenation: ", t.value()) << "\n";
			prods_t r{};
			for (auto& f : (t || tgf_parser::factor)())
				r = r + factor(sym, f);
			return r;
		}
		prods_t conjunction(const prods_t& sym,	const traverser_t& t) {
			//print_node(std::cout << "conjunction: ", t.value()) << "\n";
			prods_t r{};
			for (auto& c : (t || tgf_parser::concatenation)())
				r = r & concatenation(sym, c);
			return r;
		}
		prods_t get_new_name(const prods_t& sym) {
			std::stringstream ss;
			ss << "__E_" << sym.to_lit().to_std_string() << "_" << id++;
			//std::cout << "new name: " << id << " " << to_std_string(ss.str()) << "\n";
			return prods_t(nts(from_str<C>(ss.str())));
		}
	};
};

} // idni namespace
#endif // __IDNI__PARSER__TGF_H__

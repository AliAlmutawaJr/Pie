#pragma once

#include <numeric>


#include "../Utils/utils.hxx"
#include "../Utils/Exceptions.hxx"
#include "../Declarations.hxx"
#include "../Expr/Expr.hxx"


namespace pie {

namespace prec {
  inline constexpr auto BASE = 1 << 10; // 1024

  inline constexpr auto              LOW_VALUE = 0;
  inline constexpr auto          KEYWORD_VALUE = 				  BASE     ;
  inline constexpr auto            COMMA_VALUE = 		 KEYWORD_VALUE << 1;
  inline constexpr auto       ASSIGNMENT_VALUE =            COMMA_VALUE << 1;
  inline constexpr auto          CASCADE_VALUE =       ASSIGNMENT_VALUE << 1;
  inline constexpr auto               OR_VALUE =          CASCADE_VALUE << 1;
  inline constexpr auto              AND_VALUE =               OR_VALUE << 1;
  inline constexpr auto            BITOR_VALUE =              AND_VALUE << 1;
  inline constexpr auto           BITXOR_VALUE =            BITOR_VALUE << 1;
  inline constexpr auto           BITAND_VALUE =           BITXOR_VALUE << 1;
  inline constexpr auto               EQ_VALUE =           BITAND_VALUE << 1;
  inline constexpr auto              CMP_VALUE =               EQ_VALUE << 1;
  inline constexpr auto        SPACESHIP_VALUE =              CMP_VALUE << 1;
  inline constexpr auto            SHIFT_VALUE =        SPACESHIP_VALUE << 1;
  inline constexpr auto              SUM_VALUE =            SHIFT_VALUE << 1;
  inline constexpr auto             PROD_VALUE =              SUM_VALUE << 1;
  inline constexpr auto               AS_VALUE =             PROD_VALUE << 1;
  inline constexpr auto           PREFIX_VALUE =               AS_VALUE << 1;
  inline constexpr auto           SUFFIX_VALUE =           PREFIX_VALUE << 1;
  inline constexpr auto             CALL_VALUE =           SUFFIX_VALUE	   ; // same as a suffix operator
  inline constexpr auto    MEMBER_ACCESS_VALUE =             CALL_VALUE     ; // same as a call operator
  inline constexpr auto SCOPE_RESOLUTION_VALUE =    MEMBER_ACCESS_VALUE << 1;
  inline constexpr auto             HIGH_VALUE = SCOPE_RESOLUTION_VALUE << 1;


  inline constexpr auto LOW                = "LOW"    ;
  inline constexpr auto KEYWORD            = "keyword";
  inline constexpr auto ASSIGNMENT         = "="      ;
  inline constexpr auto OR                 = "||"     ;
  inline constexpr auto AND                = "&&"     ;
  inline constexpr auto BITOR              = "|"      ;
  inline constexpr auto BITXOR             = "^"      ;
  inline constexpr auto BITAND             = "&"      ;
  inline constexpr auto EQ                 = "=="     ;
  inline constexpr auto CMP                = "<"      ;
  inline constexpr auto SPACESHIP          = "<=>"    ;
  inline constexpr auto SHIFT              = "<<"     ;
  inline constexpr auto SUM                = "+"      ;
  inline constexpr auto PROD               = "*"      ;
  inline constexpr auto AS                 = "as"     ;
  inline constexpr auto PREFIX             = "!"      ;
  inline constexpr auto SUFFIX             = "[]"     ;
  inline constexpr auto CALL               = "()"     ;
  inline constexpr auto MEMBER_ACCESS      = "."     ;
  inline constexpr auto SCOPE_RESOLUTION   = "::"     ;
  inline constexpr auto HIGH               = "HIGH"   ;



//todo: continue refactoring!!!!!!!
  	inline int precedenceOf(const std::string& p, const Operators& ops) {
		if (p == LOW)
			return LOW_VALUE;

		if (p == KEYWORD)
			return KEYWORD_VALUE;

		if (p == ASSIGNMENT)
			return ASSIGNMENT_VALUE;

		if (p == OR)
			return OR_VALUE;

		if (p == AND)
			return AND_VALUE;

		if (p == BITOR)
			return BITOR_VALUE;

		if (p == BITXOR)
			return BITXOR_VALUE;

		if (p == BITAND)
			return BITAND_VALUE;

		if (p == "!=" or p == "==")
			return EQ_VALUE;

		if (p == ">" or p == ">=" or p == "<" or p == "<=")
			return CMP_VALUE;

		if (p == "<=>")
			return SPACESHIP_VALUE;

		if (p == "<<" or p == ">>")
			return SHIFT_VALUE;

		if (p == "+" or p == "-")
			return SUM_VALUE;

		if (p == "*" or p == "/" or p == "%")
			return PROD_VALUE;

		if (p == "as" or p == "is")
			return AS_VALUE;

		if (p == "!" or p == "~")
			return PREFIX_VALUE;

		if (p == "[]")
			return SUFFIX_VALUE;

		if (p == "()")
			return CALL_VALUE;

        if (p == ".")
            return MEMBER_ACCESS_VALUE;

		if (p == "::")
			return SCOPE_RESOLUTION_VALUE;

		if (p == "HIGH")
			return HIGH_VALUE;

		if (not ops.contains(p)) pie::util::error('\'' + p + "' does not name any operator name or precedende level!");

		// const auto& op = ops.at(p);
		// explicit type to shut up clangd about `#include "Expr.hxx"` not being used directly
		const std::shared_ptr<expr::Fix>& op = ops.at(p);
		return std::midpoint(precedenceOf(op->high, ops), precedenceOf(op->low, ops));
  	}

  	inline auto calculate(const std::string& high, const std::string& low, const Operators& ops) {
    	return std::midpoint(precedenceOf(high, ops), precedenceOf(low, ops));
  	}


	inline std::string higher(const std::string& p, const Operators& ops) {
		if (p == "LOW")             return "keyword";
		if (p == "keyword")         return "=";
		if (p == "=")               return "..";
		if (p == "..")              return "||";
		if (p == "||")              return "&&";
		if (p == "&&")              return "|";
		if (p == "|")               return "^";
		if (p == "^")               return "&";
		if (p == "&")               return "==";
		if (p == "!=" or p == "==") return "<=";

		if (p == ">" or p == ">=" or p == "<" or p == "<=" or p == "is" or p == "as") return "<=>";

		if (p == "<=>")                       return ">>";
		if (p == "<<" or p == ">>")           return "-";
		if (p == "+" or p == "-")             return "%";
		if (p == "*" or p == "/" or p == "%") return "~";
		if (p == "!" or p == "~")             return "[]";
		if (p == "[]" or p == "?")            return "()";
		if (p == "()")                        return "::";
		if (p == "::")                        return "HIGH";
		if (p == "HIGH") pie::util::error("Can't go higher than HIGH!");

		// should I assume it already contains?
		if (not ops.contains(p)) pie::util::error<except::OperatorError>('\'' + p + "' not name any precedende level or operator!");

		const auto& op = ops.at(p);
		return op->high == op->low ? higher(op->low /*or op->high*/, ops) : op->high;
	}

	inline std::string lower(const std::string& p, const Operators& ops) {
		if (p == "LOW") pie::util::error("Can't go lower than LOW!");
		if (p == "keyword")         return "LOW";
		if (p == "=")               return "keyword";
		if (p == "..")              return "=";
		if (p == "||")              return "..";
		if (p == "&&")              return "||";
		if (p == "|")               return "&&";
		if (p == "^")               return "|";
		if (p == "&")               return "^";
		if (p == "!=" or p == "==") return "&";

		if (p == ">" or p == ">=" or p == "<" or p == "<=" or p == "is" or p == "as") return "==";

		if (p == "<=>")                       return "<=";
		if (p == "<<" or p == ">>")           return "<=>";
		if (p == "+" or p == "-")             return ">>";
		if (p == "*" or p == "/" or p == "%") return "-";
		if (p == "!" or p == "~")             return "%";
		if (p == "[]" or p == "?")            return "~";
		if (p == "()")                        return "[]";
		if (p == "::")                        return "()";
		if (p == "HIGH")                      return "::";


		//? should I assume it already contains?
		if (not ops.contains(p)) pie::util::error('\'' + p + "' not name any precedende level or operator!");
		const auto& op = ops.at(p);
		return op->high == op->low ? lower(op->high /*or op->high*/, ops) : op->low;
	}

} // namespace prec
} // namespace pue
//#define BOOST_SPIRIT_X3_DEBUG
//#define DEBUG_SYMBOLS
#include <cstdlib>
#include <iostream>
#include <functional>
#include <iomanip>
#include <list>
#include <boost/fusion/adapted/struct.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <boost/foreach.hpp>

namespace x3 = boost::spirit::x3;

namespace client {
namespace ast {
    struct nil {};
    struct unary_op;
    struct binary_op;
    struct conditional_op;
    struct expression;

    using UnFunc = std::function<double(double)>;
    using BinFunc = std::function<double(double, double)>;

    struct operand : x3::variant<
                         nil,
                         double,
                         std::string,
                         x3::forward_ast<unary_op>,
                         x3::forward_ast<binary_op>,
                         x3::forward_ast<conditional_op>,
                         x3::forward_ast<expression>> {
        using base_type::base_type;
        using base_type::operator=;
    };

    struct unary_op {
        UnFunc op;
        operand rhs;
    };

    struct binary_op {
        BinFunc op;
        operand lhs;
        operand rhs;
    };

    struct conditional_op {
        operand lhs;
        operand rhs_true;
        operand rhs_false;
    };

    struct operation {
        BinFunc op;
        operand rhs;
    };

    struct expression {
        operand lhs;
        std::list<operation> rhs;
    };

} // namespace ast
} // namespace client

BOOST_FUSION_ADAPT_STRUCT(client::ast::expression, lhs, rhs)
BOOST_FUSION_ADAPT_STRUCT(client::ast::operation, op, rhs)
BOOST_FUSION_ADAPT_STRUCT(client::ast::conditional_op, lhs, rhs_true, rhs_false)
BOOST_FUSION_ADAPT_STRUCT(client::ast::binary_op, op, lhs, rhs)
BOOST_FUSION_ADAPT_STRUCT(client::ast::unary_op, op, rhs)

namespace client {
namespace parser {
    struct error_handler {
        template <typename It, typename Ctx>
        x3::error_handler_result
        on_error(It f, It l, x3::expectation_failure<It> const& e, Ctx const&)
            const {
            std::cout << std::string(f, l) << "\n"
                      << std::setw(1 + std::distance(f, e.where())) << "^"
                      << "-- expected: " << e.which() << "\n";
            return x3::error_handler_result::fail;
        }
    };

    struct expression_class : error_handler {};
    struct logical_class : error_handler {};
    struct equality_class : error_handler {};
    struct relational_class : error_handler {};
    struct additive_class : error_handler {};
    struct multiplicative_class : error_handler {};
    struct factor_class : error_handler {};
    struct primary_class : error_handler {};
    struct unary_class : error_handler {};
    struct binary_class : error_handler {};
    struct conditional_class : error_handler {};
    struct variable_class : error_handler {};

    // Rule declarations
    auto const expression =
        x3::rule<expression_class, ast::operand>{"expression"};
    auto const conditional =
        x3::rule<conditional_class, ast::operand>{"conditional"};
    auto const primary = x3::rule<primary_class, ast::operand>{"primary"};
    auto const logical = x3::rule<logical_class, ast::expression>{"logical"};
    auto const equality = x3::rule<equality_class, ast::expression>{"equality"};
    auto const relational =
        x3::rule<relational_class, ast::expression>{"relational"};
    auto const additive = x3::rule<additive_class, ast::expression>{"additive"};
    auto const multiplicative =
        x3::rule<multiplicative_class, ast::expression>{"multiplicative"};
    auto const factor = x3::rule<factor_class, ast::expression>{"factor"};
    auto const unary = x3::rule<unary_class, ast::unary_op>{"unary"};
    auto const binary = x3::rule<binary_class, ast::binary_op>{"binary"};
    auto const variable = x3::rule<variable_class, std::string>{"variable"};

    struct constant_ : x3::symbols<double> {
        constant_() {
            this->add("e", boost::math::constants::e<double>())(
                "pi", boost::math::constants::pi<double>());
        }
    } constant;

    struct ufunc_ : x3::symbols<ast::UnFunc> {
        ufunc_() {
            this->add("abs", &std::abs<double>);
        }
    } ufunc;

    struct bfunc_ : x3::symbols<ast::BinFunc> {
        bfunc_() {
            this->add("max", [](double a, double b) {
                return std::fmax(a, b);
            })("min", [](double a, double b) {
                return std::fmin(a, b);
            })("pow", [](double a, double b) { return std::pow(a, b); });
        }
    } bfunc;

    struct unary_op_ : x3::symbols<ast::UnFunc> {
        unary_op_() {
            this->add("+", [](double v) { return +v; })(
                "-", std::negate<double>{})("!", [](double v) { return !v; });
        }
    } unary_op;

    struct additive_op_ : x3::symbols<ast::BinFunc> {
        additive_op_() {
            this->add("+", std::plus<double>{})("-", std::minus<double>{});
        }
    } additive_op;

    struct multiplicative_op_ : x3::symbols<ast::BinFunc> {
        multiplicative_op_() {
            this->add("*", std::multiplies<>{})("/", std::divides<>{})(
                "%", [](double a, double b) { return std::fmod(a, b); });
        }
    } multiplicative_op;

    struct logical_op_ : x3::symbols<ast::BinFunc> {
        logical_op_() {
            this->add("&&", std::logical_and<double>{})(
                "||", std::logical_or<double>{});
        }
    } logical_op;

    struct relational_op_ : x3::symbols<ast::BinFunc> {
        relational_op_() {
            this->add("<", std::less<double>{})(
                "<=", std::less_equal<double>{})(">", std::greater<double>{})(
                ">=", std::greater_equal<double>{});
        }
    } relational_op;

    struct equality_op_ : x3::symbols<ast::BinFunc> {
        equality_op_() {
            this->add("==", std::equal_to<double>{})(
                "!=", std::not_equal_to<double>{});
        }
    } equality_op;

    struct power_ : x3::symbols<ast::BinFunc> {
        power_() {
            this->add(
                "**", [](double v, double exp) { return std::pow(v, exp); });
        }
    } power;

    auto const variable_def = x3::lexeme[x3::alpha >> *x3::alnum];

    // Rule defintions
    auto const expression_def = conditional;

    auto make_conditional_op = [](auto& ctx) {
        using boost::fusion::at_c;
        x3::_val(ctx) = ast::conditional_op{
            x3::_val(ctx), at_c<0>(x3::_attr(ctx)), at_c<1>(x3::_attr(ctx))};
    };

    auto const conditional_def =
        logical[([](auto& ctx) { _val(ctx) = _attr(ctx); })] >>
        -('?' > expression > ':' > expression)[make_conditional_op];

    auto const logical_def = equality >> *(logical_op > equality);
    auto const equality_def = relational >> *(equality_op > relational);
    auto const relational_def = additive >> *(relational_op > additive);
    auto const additive_def = multiplicative >> *(additive_op > multiplicative);
    auto const multiplicative_def = factor >> *(multiplicative_op > factor);
    auto const factor_def = primary >> *(power > factor);
    auto const unary_def =
        (unary_op > primary) | (ufunc > '(' > expression > ')');
    auto const binary_def = bfunc > '(' > expression > ',' > expression > ')';
    auto const primary_def = x3::double_ | ('(' > expression > ')') | binary |
                             unary | constant | variable;

    BOOST_SPIRIT_DEFINE(
        expression,
        logical,
        equality,
        relational,
        additive,
        multiplicative,
        factor,
        primary,
        unary,
        binary,
        conditional,
        variable);
} // namespace parser
} // namespace client

namespace client {
namespace ast {
    struct evaluator {
        typedef double result_type;

        evaluator(const double variable) : variable(variable) {}
        double variable;

        double
        operator()(operand const& ast) const {
            return boost::apply_visitor(*this, ast.get());
        }

        double
        operator()(nil) const {
            BOOST_ASSERT(0);
            return 0;
        }

        double
        operator()(expression const& ast) const {
            double state = boost::apply_visitor(*this, ast.lhs);
            BOOST_FOREACH (operation const& op, ast.rhs) {
                state = (*this)(op, state);
            }
            return state;
        }

        double
        operator()(operation const& ast, double lhs) const {
            double rhs = boost::apply_visitor(*this, ast.rhs);
            return ast.op(lhs, rhs);
        }

        double
        operator()(unary_op const& ast) const {
            double rhs = boost::apply_visitor(*this, ast.rhs);
            return ast.op(rhs);
        }

        double
        operator()(binary_op const& ast) const {
            double rhs = boost::apply_visitor(*this, ast.rhs);
            double lhs = boost::apply_visitor(*this, ast.lhs);
            return ast.op(lhs, rhs);
        }

        double
        operator()(conditional_op const& ast) const {
            bool lhs = boost::apply_visitor(*this, ast.lhs);
            if (lhs) {
                return boost::apply_visitor(*this, ast.rhs_true);
            }
            return boost::apply_visitor(*this, ast.rhs_false);
        }

        double
        operator()(double const& ast) const {
            return ast;
        }

        double
        operator()(std::string const& ast) const {
            return variable;
        }
    };
} // namespace ast
} // namespace client

int
main() {
    std::list<std::string> test_expressions{
        "0",
        "(n == 0) ? 0 : ((n == 1) ? 1 : 2)",
        "(n == 0) ? 0 : ((n == 1) ? 1 : (((n % 100 == 2 || n % 100 == 22 || n "
        "% 100 == 42 || n % 100 == 62 || n % 100 == 82) || n % 1000 == 0 && (n "
        "% 100000 >= 1000 && n % 100000 <= 20000 || n % 100000 == 40000 || n % "
        "100000 == 60000 || n % 100000 == 80000) || n != 0 && n % 1000000 == "
        "100000) ? 2 : ((n % 100 == 3 || n % 100 == 23 || n % 100 == 43 || n % "
        "100 == 63 || n % 100 == 83) ? 3 : ((n != 1 && (n % 100 == 1 || n % "
        "100 == 21 || n % 100 == 41 || n % 100 == 61 || n % 100 == 81)) ? 4 : "
        "5))))",
        "(n == 0) ? 0 : ((n == 1) ? 1 : ((n == 2) ? 2 : ((n % 100 >= 3 && n % "
        "100 <= 10) ? 3 : ((n % 100 >= 11 && n % 100 <= 99) ? 4 : 5))))",
        "(n == 0) ? 0 : ((n == 1) ? 1 : ((n == 2) ? 2 : ((n == 3) ? 3 : ((n == "
        "6) ? 4 : 5))))",
        "(n == 0 || n == 1) ? 0 : ((n >= 2 && n <= 10) ? 1 : 2)",
        "n != 1",
        "n > 1",
        "(n % 100 == 1) ? 0 : ((n % 100 == 2) ? 1 : ((n % 100 == 3 || n % 100 "
        "== 4) ? 2 : 3))",
        "(n % 10 == 0 || n % 100 >= 11 && n % 100 <= 19) ? 0 : ((n % 10 == 1 "
        "&& n % 100 != 11) ? 1 : 2)",
        "(n % 10 == 1) ? 0 : ((n % 10 == 2) ? 1 : ((n % 100 == 0 || n % 100 == "
        "20 || n % 100 == 40 || n % 100 == 60 || n % 100 == 80) ? 2 : 3))",
        "n % 10 != 1 || n % 100 == 11",
        "(n % 10 == 1 && n % 100 != 11) ? 0 : ((n % 10 >= 2 && n % 10 <= 4 && "
        "(n % 100 < 12 || n % 100 > 14)) ? 1 : 2)",
        "(n % 10 == 1 && (n % 100 < 11 || n % 100 > 19)) ? 0 : ((n % 10 >= 2 "
        "&& n % 10 <= 9 && (n % 100 < 11 || n % 100 > 19)) ? 1 : 2)",
        "(n % 10 == 1 && n % 100 != 11 && n % 100 != 71 && n % 100 != 91) ? 0 "
        ": ((n % 10 == 2 && n % 100 != 12 && n % 100 != 72 && n % 100 != 92) ? "
        "1 : ((((n % 10 == 3 || n % 10 == 4) || n % 10 == 9) && (n % 100 < 10 "
        "|| n % 100 > 19) && (n % 100 < 70 || n % 100 > 79) && (n % 100 < 90 "
        "|| n % 100 > 99)) ? 2 : ((n != 0 && n % 1000000 == 0) ? 3 : 4)))",
        "(n == 1) ? 0 : ((n == 0 || n % 100 >= 2 && n % 100 <= 10) ? 1 : ((n % "
        "100 >= 11 && n % 100 <= 19) ? 2 : 3))",
        "(n == 1) ? 0 : ((n == 0 || n % 100 >= 2 && n % 100 <= 19) ? 1 : 2)",
        "(n == 1) ? 0 : ((n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 12 || n % "
        "100 > 14)) ? 1 : 2)",
        "(n == 1) ? 0 : ((n == 2) ? 1 : 2)",
        "(n == 1) ? 0 : ((n == 2) ? 1 : ((n > 10 && n % 10 == 0) ? 2 : 3))",
        "(n == 1) ? 0 : ((n == 2) ? 1 : ((n >= 3 && n <= 6) ? 2 : ((n >= 7 && "
        "n <= 10) ? 3 : 4)))",
        "(n == 1) ? 0 : ((n >= 2 && n <= 4) ? 1 : 2)",
        "(n == 1 || n == 11) ? 0 : ((n == 2 || n == 12) ? 1 : ((n >= 3 && n <= "
        "10 || n >= 13 && n <= 19) ? 2 : 3))",
        "n != 1 && n != 2 && n != 3 && (n % 10 == 4 || n % 10 == 6 || n % 10 "
        "== 9)",
        "n >= 2 && (n < 11 || n > 99)",
    };

    for (const std::string& str : test_expressions) {
        for (double idx = 0; idx <= 1000; ++idx) {
            std::cout << "-------------------------" << std::endl;
            std::cout << "Expression: " << std::quoted(str) << std::endl;
            std::cout << "n: " << idx << std::endl;
            client::ast::operand program;
            client::ast::evaluator eval(idx);

            std::string::const_iterator iter = str.begin();
            std::string::const_iterator end = str.end();
            bool r = phrase_parse(
                iter, end, client::parser::expression, x3::space, program);

            if (r && iter == end) {
                std::cout << "Parsing succeeded" << std::endl;
                std::cout << "Result: " << eval(program) << std::endl;
                std::cout << "-------------------------" << std::endl;
            } else {
                std::string rest(iter, end);
                std::cout << "Parsing failed" << std::endl;
                std::cout << "stopped at: \" " << rest << "\"" << std::endl;
                std::cout << "-------------------------" << std::endl;
                return EXIT_FAILURE;
            }
        }
    }

    std::cout << "///////////////////////////////////////////////////\n\n";
    std::cout << "Expression parser...\n\n";
    std::cout << "///////////////////////////////////////////////////\n\n";

    std::string str;
    while (true) {
        std::cout << "Type an expression...or [q or Q] to quit\n\n";
        std::getline(std::cin, str);

        if (str.empty() || str[0] == 'q' || str[0] == 'Q')
            break;

        std::cout << "Enter variable value: ";
        std::string variable_str;
        std::getline(std::cin, variable_str);
        double variable = std::stod(variable_str);

        client::ast::operand program;
        client::ast::evaluator eval(variable);

        std::string::const_iterator iter = str.begin();
        std::string::const_iterator end = str.end();
        bool r = phrase_parse(
            iter, end, client::parser::expression, x3::space, program);

        if (r && iter == end) {
            std::cout << "-------------------------" << std::endl;
            std::cout << "Parsing succeeded" << std::endl;
            std::cout << "Result: " << eval(program) << std::endl;
            std::cout << "-------------------------" << std::endl;
        } else {
            std::string rest(iter, end);
            std::cout << "-------------------------" << std::endl;
            std::cout << "Parsing failed" << std::endl;
            std::cout << "stopped at: \" " << rest << "\"" << std::endl;
            std::cout << "-------------------------" << std::endl;
        }
    }

    std::cout << "Bye... :-) \n\n";
    return EXIT_SUCCESS;
}

/**
 * @file
 * Declares the Puppet language grammar.
 * The grammar defines the Puppet language and is responsible for populating a syntax tree.
 */
#pragma once

#include "token_pos.hpp"
#include "../lexer/token_id.hpp"
#include "../ast/syntax_tree.hpp"
#include "../cast.hpp"
#include <boost/spirit/include/qi.hpp>
#include <boost/phoenix.hpp>

namespace puppet { namespace compiler {

    /**
     * Represents the Puppet language grammar.
     * The grammar is responsible for transforming a stream of tokens into a syntax tree.
     * @tparam Lexer The lexer type to use for token definitions.
     */
    template <typename Lexer>
    struct grammar : boost::spirit::qi::grammar<typename Lexer::iterator_type, puppet::ast::syntax_tree()>
    {
        /**
         * The token iterator type of the grammar.
         */
        typedef typename Lexer::iterator_type iterator_type;

        /**
         * Constructs a Puppet language grammar for the given lexer.
         * @param lexer The lexer to use for token definitions.
         * @param interpolation True if the grammar is being used for string interpolation or false if not.
         */
        grammar(Lexer const& lexer, bool interpolation = false) :
            boost::spirit::qi::grammar<iterator_type, puppet::ast::syntax_tree()>(syntax_tree)
        {
            using namespace boost::spirit::qi;
            using namespace puppet::lexer;
            namespace phx = boost::phoenix;
            
            // A syntax tree is a sequence of statements
            // For string interpolation, end at the first '}' token that isn't part of the grammar
            if (interpolation) {
                syntax_tree =
                    (raw_token('{') > statements > token_pos('}')) [_val = phx::construct<ast::syntax_tree>(_1, _2)];
            } else {
                syntax_tree =
                    statements [ _val = phx::construct<ast::syntax_tree>(_1) ];
            }

            // Statements
            // The Puppet language doesn't really have "statements" in a pedantic sense of the word
            // It considers a statement to be "any expression"
            statements =
                -(statement % -raw_token(';')) > -raw_token(';');
            statement =
                (statement_expression > *binary_statement) [ _val = phx::construct<ast::expression>(_1, _2) ];
            statement_expression =
                (
                    resource_expression          |
                    resource_defaults_expression |
                    resource_override_expression |
                    class_definition_expression  |
                    defined_type_expression      |
                    node_definition_expression
                )                         [ _val = phx::construct<ast::catalog_expression>(_1) ]      |
                statement_call_expression [ _val = phx::construct<ast::control_flow_expression>(_1) ] |
                primary_expression        [ _val = _1 ];
            binary_statement =
                (binary_operator > statement_expression) [ _val = phx::construct<ast::binary_expression>(_1, _2) ];

            // Expressions
            // Expressions in Puppet do not include certain resource expressions (resource, defaults, and override expressions)
            // Those expressions are only available as "statements"
            expressions =
                (expression % raw_token(',')) > -raw_token(',');
            expression =
                (primary_expression > *binary_expression) [ _val = phx::construct<ast::expression>(_1, _2) ];

            // Primary expression
            // The order of the subexpressions is important; specifically basic_expression must come after the other subexpressions
            primary_expression =
                (
                    (
                        unary_expression |
                        catalog_expression |
                        control_flow_expression |
                        basic_expression |
                        (raw_token('(') > expression > raw_token(')'))
                    ) [ _a = _1 ] >>
                        -(
                            (+postfix_subexpression) [ _a = phx::construct<ast::postfix_expression>(_a, _1) ]
                         )
                ) [ _val = _a ];

            // Basic expressions
            basic_expression =
                undef     |
                defaulted |
                boolean   |
                number    |
                string    |
                regex     |
                variable  |
                name      |
                bare_word |
                type      |
                array     |
                hash;
            undef =
                token_pos(token_id::keyword_undef) [ _val = phx::construct<ast::undef>(_1) ];
            defaulted =
                token_pos(token_id::keyword_default) [ _val = phx::construct<ast::defaulted>(_1) ];
            boolean =
                token_pos(token_id::keyword_true)  [ _val = phx::construct<ast::boolean>(_1, true) ] |
                token_pos(token_id::keyword_false) [ _val = phx::construct<ast::boolean>(_1, false) ];
            number =
                lexer.number [ _val = phx::construct<ast::number>(_1) ];
            string =
                (lexer.single_quoted_string | lexer.double_quoted_string | lexer.heredoc) [ _val = phx::construct<ast::string>(_1) ];
            regex =
                token(token_id::regex) [ _val = phx::construct<ast::regex>(_1) ];
            variable =
                token(token_id::variable) [ _val = phx::construct<ast::variable>(_1) ];
            name =
                (token(token_id::name) | token(token_id::statement_call)) [ _val = phx::construct<ast::name>(_1) ];
            bare_word =
                token(token_id::bare_word) [ _val = phx::construct<ast::bare_word>(_1) ];
            type =
                token(token_id::type) [ _val = phx::construct<ast::type>(_1) ];
            array =
                ((token_pos('[') | token_pos(token_id::array_start)) > -expressions > raw_token(']')) [ _val = phx::construct<ast::array>(_1, _2) ];
            hash =
                (token_pos('{') > -(hash_pair % raw_token(',')) > -raw_token(',') > raw_token('}')) [ _val = phx::construct<ast::hash>(_1, _2) ];
            hash_pair =
                (expression > raw_token(token_id::fat_arrow) > expression) [ _val = phx::construct<ast::hash_pair>(_1, _2) ];

            // Control-flow expressions
            control_flow_expression =
                // Selector and method call expressions are postfix
                case_expression |
                if_expression |
                unless_expression |
                function_call_expression;
            case_expression =
                (token_pos(token_id::keyword_case) > expression > raw_token('{') > +case_proposition > raw_token('}')) [ _val = phx::construct<ast::case_expression>(_1, _2, _3) ];
            case_proposition =
                (expressions > raw_token(':') > raw_token('{') > statements > raw_token('}')) [ _val = phx::construct<ast::case_proposition>(_1, _2) ];
            if_expression =
                (token_pos(token_id::keyword_if) > expression > raw_token('{') > statements > raw_token('}') > *elsif_expression > -else_expression) [ _val = phx::construct<ast::if_expression>(_1, _2, _3, _4, _5) ];
            elsif_expression =
                (token_pos(token_id::keyword_elsif) > expression > raw_token('{') > statements > raw_token('}')) [ _val = phx::construct<ast::elsif_expression>(_1, _2, _3) ];
            else_expression =
                (token_pos(token_id::keyword_else) > raw_token('{') > statements > raw_token('}')) [ _val = phx::construct<ast::else_expression>(_1, _2) ];
            unless_expression =
                (token_pos(token_id::keyword_unless) > expression > raw_token('{') > statements > raw_token('}') > -else_expression) [ _val = phx::construct<ast::unless_expression>(_1, _2, _3, _4) ];
            function_call_expression =
                ((name >> raw_token('(')) > -expressions > raw_token(')') > -lambda) [ _val = phx::construct<ast::function_call_expression>(_1, _2, _3) ];
            statement_call_expression =
                (token(token_id::statement_call) >> !raw_token('(') >> expressions >> -lambda) [ _val = phx::construct<ast::function_call_expression>(phx::construct<ast::name>(_1), _2, _3) ];
            lambda =
                (token_pos('|') > -(parameter % raw_token(',')) > -raw_token(',') > raw_token('|') > raw_token('{') > statements > raw_token('}')) [ _val = phx::construct<ast::lambda>(_1, _2, _3) ];
            parameter =
                (-type_expression >> matches[raw_token('*')] >> variable >> -(raw_token('=') > expression)) [ _val = phx::construct<ast::parameter>(_1, _2, _3, _4) ];

            // Catalog expressions
            catalog_expression =
                // Everything but collection expressions are statement-level only
                collection_expression;
            resource_expression =
                (raw_token('@') > resource_type > raw_token('{') > (resource_body % raw_token(';')) > -raw_token(';') > raw_token('}'))            [ _val = phx::construct<ast::resource_expression>(_1, _2, ast::resource_status::virtualized) ] |
                (raw_token(token_id::atat) > resource_type > raw_token('{') > (resource_body % raw_token(';')) > -raw_token(';') > raw_token('}')) [ _val = phx::construct<ast::resource_expression>(_1, _2, ast::resource_status::exported) ] |
                ((resource_type >> raw_token('{')) >> (resource_body % raw_token(';')) > -raw_token(';') > raw_token('}'))                         [ _val = phx::construct<ast::resource_expression>(_1, _2) ];
            resource_type =
                name                           [ _val = phx::construct<ast::basic_expression>(_1) ] |
                token(token_id::keyword_class) [ _val = phx::construct<ast::basic_expression>(phx::construct<ast::name>(_1)) ] |
                type_expression                [ _val = _1 ];
            resource_body =
                ((expression >> raw_token(':')) > -(attribute_expression % raw_token(',')) > -raw_token(',')) [ _val = phx::construct<ast::resource_body>(_1, _2) ];
            attribute_expression =
                (attribute_name > attribute_operator > expression) [ _val = phx::construct<ast::attribute_expression>(_1, _2, _3) ];
            attribute_operator =
                raw_token(token_id::fat_arrow)  [ _val = ast::attribute_operator::assignment ] |
                raw_token(token_id::plus_arrow) [ _val = ast::attribute_operator::append ];
            attribute_name =
                (
                    token(token_id::name)             |
                    token(token_id::statement_call)   |
                    token(token_id::keyword_and)      |
                    token(token_id::keyword_case)     |
                    token(token_id::keyword_class)    |
                    token(token_id::keyword_default)  |
                    token(token_id::keyword_define)   |
                    token(token_id::keyword_else)     |
                    token(token_id::keyword_elsif)    |
                    token(token_id::keyword_if)       |
                    token(token_id::keyword_in)       |
                    token(token_id::keyword_inherits) |
                    token(token_id::keyword_node)     |
                    token(token_id::keyword_or)       |
                    token(token_id::keyword_undef)    |
                    token(token_id::keyword_unless)   |
                    token(token_id::keyword_type)     |
                    token(token_id::keyword_attr)     |
                    token(token_id::keyword_function) |
                    token(token_id::keyword_private)
                ) [ _val = phx::construct<ast::name>(_1) ];
            resource_defaults_expression =
                ((type >> raw_token('{')) > -(attribute_expression % raw_token(',')) > -raw_token(',') > raw_token('}')) [ _val = phx::construct<ast::resource_defaults_expression>(_1, _2) ];
            resource_override_expression =
                ((variable_type_expression >> raw_token('{')) > -(attribute_expression % raw_token(',')) > -raw_token(',') > raw_token('}')) [ _val = phx::construct<ast::resource_override_expression>(_1, _2) ];
            class_definition_expression =
                (token_pos(token_id::keyword_class) > name > -(raw_token('(') > -(parameter % raw_token(',')) > -raw_token(',') > raw_token(')')) > -(raw_token(token_id::keyword_inherits) > name) > raw_token('{') > -statements > raw_token('}')) [ _val = phx::construct<ast::class_definition_expression>(_1, _2, _3, _4, _5) ];
            defined_type_expression =
                (token_pos(token_id::keyword_define) > name > -(raw_token('(') > -(parameter % raw_token(',')) > -raw_token(',') > raw_token(')')) > raw_token('{') > -statements > raw_token('}')) [ _val = phx::construct<ast::defined_type_expression>(_1, _2, _3, _4) ];
            node_definition_expression =
                (token_pos(token_id::keyword_node) > (hostname % ',') > -raw_token(',') > raw_token('{') > statements > raw_token('}')) [ _val = phx::construct<ast::node_definition_expression>(_1, _2, _3) ];
            hostname =
                string                                         [ _val = phx::construct<ast::hostname>(_1) ] |
                defaulted                                      [ _val = phx::construct<ast::hostname>(_1) ] |
                regex                                          [ _val = phx::construct<ast::hostname>(_1) ] |
                ((name | bare_word | number) % raw_token('.')) [ _val = phx::construct<ast::hostname>(_1) ];
            collection_expression =
                ((type >> raw_token(token_id::left_collect)) > -query > *binary_query_expression > raw_token(token_id::right_collect))               [ _val = phx::construct<ast::collection_expression>(ast::collection_kind::all, _1, _2, _3) ] |
                ((type >> raw_token(token_id::left_double_collect)) > -query > *binary_query_expression > raw_token(token_id::right_double_collect)) [ _val = phx::construct<ast::collection_expression>(ast::collection_kind::exported, _1, _2, _3) ];
            binary_query_expression =
                (binary_query_operator > query) [ _val = phx::construct<ast::binary_query_expression>(_1, _2) ];
            binary_query_operator =
                raw_token(token_id::keyword_and) [ _val = ast::binary_query_operator::logical_and ] |
                raw_token(token_id::keyword_or)  [ _val = ast::binary_query_operator::logical_or ];
            query =
                (name > attribute_query_operator > attribute_query_value) [ _val = phx::construct<ast::query>(_1, _2, _3) ];
            attribute_query_operator =
                raw_token(token_id::equals)     [ _val = ast::attribute_query_operator::equals ] |
                raw_token(token_id::not_equals) [ _val = ast::attribute_query_operator::not_equals ];
            attribute_query_value =
                variable |
                string   |
                boolean  |
                number   |
                name;

            // Unary expressions
            unary_expression =
                (token_pos('-') > primary_expression) [ _val = phx::construct<ast::unary_expression>(_1, ast::unary_operator::negate, _2) ] |
                (token_pos('*') > primary_expression) [ _val = phx::construct<ast::unary_expression>(_1, ast::unary_operator::splat, _2) ]  |
                (token_pos('!') > primary_expression) [ _val = phx::construct<ast::unary_expression>(_1, ast::unary_operator::logical_not, _2) ];

            // Postfix expressions
            postfix_subexpression =
                selector_expression |
                access_expression   |
                method_call_expression;
            selector_expression =
                (token_pos('?') > raw_token('{') > (selector_case_expression % raw_token(',')) > -raw_token(',') > raw_token('}')) [ _val = phx::construct<ast::selector_expression>(_1, _2) ];
            selector_case_expression =
                (expression > raw_token(token_id::fat_arrow) > expression) [ _val = phx::construct<ast::selector_case_expression>(_1, _2) ];
            access_expression =
                (token_pos('[') > expressions > raw_token(']')) [ _val = phx::construct<ast::access_expression>(_1, _2) ];
            method_call_expression =
                (raw_token('.') > name > -(raw_token('(') > expressions > raw_token(')')) > -lambda) [ _val = phx::construct<ast::method_call_expression>(_1, _2, _3) ];

            // Binary expression
            binary_expression =
                (binary_operator > primary_expression) [ _val = phx::construct<ast::binary_expression>(_1, _2) ];
            binary_operator =
                raw_token(token_id::keyword_in)     [ _val = ast::binary_operator::in ]                |
                raw_token(token_id::match)          [ _val = ast::binary_operator::match ]             |
                raw_token(token_id::not_match)      [ _val = ast::binary_operator::not_match ]         |
                raw_token('*')                      [ _val = ast::binary_operator::multiply ]          |
                raw_token('/')                      [ _val = ast::binary_operator::divide ]            |
                raw_token('%')                      [ _val = ast::binary_operator::modulo ]            |
                raw_token('+')                      [ _val = ast::binary_operator::plus ]              |
                raw_token('-')                      [ _val = ast::binary_operator::minus ]             |
                raw_token(token_id::left_shift)     [ _val = ast::binary_operator::left_shift ]        |
                raw_token(token_id::right_shift)    [ _val = ast::binary_operator::right_shift ]       |
                raw_token(token_id::equals)         [ _val = ast::binary_operator::equals ]            |
                raw_token(token_id::not_equals)     [ _val = ast::binary_operator::not_equals ]        |
                raw_token('>')                      [ _val = ast::binary_operator::greater_than ]      |
                raw_token(token_id::greater_equals) [ _val = ast::binary_operator::greater_equals ]    |
                raw_token('<')                      [ _val = ast::binary_operator::less_than ]         |
                raw_token(token_id::less_equals)    [ _val = ast::binary_operator::less_equals ]       |
                raw_token(token_id::keyword_and)    [ _val = ast::binary_operator::logical_and ]       |
                raw_token(token_id::keyword_or)     [ _val = ast::binary_operator::logical_or ]        |
                raw_token('=')                      [ _val = ast::binary_operator::assignment ]        |
                raw_token(token_id::in_edge)        [ _val = ast::binary_operator::in_edge ]           |
                raw_token(token_id::in_edge_sub)    [ _val = ast::binary_operator::in_edge_subscribe ] |
                raw_token(token_id::out_edge)       [ _val = ast::binary_operator::out_edge ]          |
                raw_token(token_id::out_edge_sub)   [ _val = ast::binary_operator::out_edge_subscribe ];

            // Type expression
            type_expression =
                (type > *type_access_expression) [ _val = phx::construct<ast::postfix_expression>(phx::construct<ast::basic_expression>(_1), _2) ];
            variable_type_expression =
                ((type | variable) > *type_access_expression) [ _val = phx::construct<ast::postfix_expression>(phx::construct<ast::basic_expression>(_1), _2) ];
            type_access_expression =
                access_expression;

            // Syntax tree
            syntax_tree.name("syntax tree");

            // Statements
            statements.name("statements");
            statement.name("statement");
            statement_expression.name("statement expression");
            binary_statement.name("binary statement");

            // Expressions
            expressions.name("expressions");
            expression.name("expression");

            // Primary expression
            primary_expression.name("primary expression");

            // Basic expressions
            basic_expression.name("basic expression");
            undef.name("undef");
            defaulted.name("default");
            boolean.name("boolean");
            number.name("number");
            string.name("string");
            regex.name("regex");
            variable.name("variable");
            name.name("name");
            bare_word.name("bare word");
            type.name("type");
            array.name("array");
            hash.name("hash");
            hash_pair.name("hash pair");

            // Control-flow expressions
            control_flow_expression.name("control flow expression");
            case_expression.name("case expression");
            case_proposition.name("case proposition");
            if_expression.name("if expression");
            elsif_expression.name("elsif expression");
            else_expression.name("else expression");
            unless_expression.name("unless expression");
            function_call_expression.name("function call expression");
            statement_call_expression.name("statement call expression");
            lambda.name("lambda");
            parameter.name("parameter");

            // Catalog expressions
            catalog_expression.name("catalog expression");
            resource_expression.name("resource expression");
            resource_type.name("resource type");
            resource_body.name("resource body");
            attribute_expression.name("attribute expression");
            attribute_operator.name("attribute operator");
            attribute_name.name("attribute name");
            resource_defaults_expression.name("resource defaults expression");
            resource_override_expression.name("resource override expression");
            class_definition_expression.name("class definition expression");
            defined_type_expression.name("defined type expression");
            node_definition_expression.name("node definition expression");
            hostname.name("hostname");
            collection_expression.name("collection expression");
            binary_query_expression.name("binary query expression");
            binary_query_operator.name("binary query operator");
            query.name("query");
            attribute_query_operator.name("attribute query operator");
            attribute_query_value.name("attribute query value");

            // Unary expressions
            unary_expression.name("unary expression");

            // Postfix expressions
            postfix_subexpression.name("postfix subexpression");
            selector_expression.name("selector expression");
            selector_case_expression.name("selector case expression");
            access_expression.name("access expression");
            method_call_expression.name("method call expression");

            // Binary expressions
            binary_expression.name("binary expression");
            binary_operator.name("binary operator");

            // Type expression
            type_expression.name("type expression");
            variable_type_expression.name("variable or type expression");
            type_access_expression.name("type access expression");

#ifdef DEBUG_GRAMMAR
            // Syntax tree
            debug(syntax_tree);

            // Statements
            debug(statements);
            debug(statement);
            debug(statement_expression);
            debug(binary_statement);

            // Expressions
            debug(expressions);
            debug(expression);

            // Primary expression
            debug(primary_expression);

            // Basic expressions
            debug(basic_expression);
            debug(undef);
            debug(defaulted);
            debug(boolean);
            debug(number);
            debug(string);
            debug(regex);
            debug(variable);
            debug(name);
            debug(bare_word);
            debug(type);
            debug(array);
            debug(hash);
            debug(hash_pair);

            // Control-flow expressions
            debug(control_flow_expression);
            debug(case_expression);
            debug(case_proposition);
            debug(if_expression);
            debug(elsif_expression);
            debug(else_expression);
            debug(unless_expression);
            debug(function_call_expression);
            debug(statement_call_expression);
            debug(lambda);
            debug(parameter);

            // Catalog expressions
            debug(catalog_expression);
            debug(resource_expression);
            debug(resource_type);
            debug(resource_body);
            debug(attribute_expression);
            debug(attribute_operator);
            debug(attribute_name);
            debug(resource_defaults_expression);
            debug(resource_override_expression);
            debug(class_definition_expression);
            debug(defined_type_expression);
            debug(node_definition_expression);
            debug(hostname);
            debug(collection_expression);
            debug(binary_query_expression);
            debug(binary_query_operator);
            debug(query);
            debug(attribute_query_operator);
            debug(attribute_query_value);

            // Unary expressions
            debug(unary_expression);

            // Postfix expressions
            debug(postfix_subexpression);
            debug(selector_expression);
            debug(selector_case_expression);
            debug(access_expression);
            debug(method_call_expression);

            // Binary expressions
            debug(binary_expression);
            debug(binary_operator);

            // Type expression
            debug(type_expression);
            debug(variable_type_expression);
            debug(type_access_expression);
#endif
        }

     private:
        // Syntax tree
        boost::spirit::qi::rule<iterator_type, puppet::ast::syntax_tree()> syntax_tree;

        // Statements
        boost::spirit::qi::rule<iterator_type, boost::optional<std::vector<puppet::ast::expression>>()> statements;
        boost::spirit::qi::rule<iterator_type, puppet::ast::expression()> statement;
        boost::spirit::qi::rule<iterator_type, puppet::ast::primary_expression()> statement_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::binary_expression()> binary_statement;

        // Expressions
        boost::spirit::qi::rule<iterator_type, std::vector<puppet::ast::expression>()> expressions;
        boost::spirit::qi::rule<iterator_type, puppet::ast::expression()> expression;

        // Primary expression
        boost::spirit::qi::rule<iterator_type, puppet::ast::primary_expression(), boost::spirit::qi::locals<puppet::ast::primary_expression>> primary_expression;

        // Basic expressions
        boost::spirit::qi::rule<iterator_type, puppet::ast::basic_expression()> basic_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::undef()> undef;
        boost::spirit::qi::rule<iterator_type, puppet::ast::defaulted()> defaulted;
        boost::spirit::qi::rule<iterator_type, puppet::ast::boolean()> boolean;
        boost::spirit::qi::rule<iterator_type, puppet::ast::number()> number;
        boost::spirit::qi::rule<iterator_type, puppet::ast::string()> string;
        boost::spirit::qi::rule<iterator_type, puppet::ast::regex()> regex;
        boost::spirit::qi::rule<iterator_type, puppet::ast::variable()> variable;
        boost::spirit::qi::rule<iterator_type, puppet::ast::name()> name;
        boost::spirit::qi::rule<iterator_type, puppet::ast::bare_word()> bare_word;
        boost::spirit::qi::rule<iterator_type, puppet::ast::type()> type;
        boost::spirit::qi::rule<iterator_type, puppet::ast::array()> array;
        boost::spirit::qi::rule<iterator_type, puppet::ast::hash()> hash;
        boost::spirit::qi::rule<iterator_type, puppet::ast::hash_pair()> hash_pair;

        // Control-flow expressions
        boost::spirit::qi::rule<iterator_type, puppet::ast::control_flow_expression()> control_flow_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::case_expression()> case_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::case_proposition()> case_proposition;
        boost::spirit::qi::rule<iterator_type, puppet::ast::if_expression()> if_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::elsif_expression()> elsif_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::else_expression()> else_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::unless_expression()> unless_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::function_call_expression()> function_call_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::function_call_expression()> statement_call_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::lambda()> lambda;
        boost::spirit::qi::rule<iterator_type, puppet::ast::parameter()> parameter;

        // Catalog expressions
        boost::spirit::qi::rule<iterator_type, puppet::ast::catalog_expression()> catalog_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::resource_expression()> resource_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::primary_expression()> resource_type;
        boost::spirit::qi::rule<iterator_type, puppet::ast::resource_body()> resource_body;
        boost::spirit::qi::rule<iterator_type, puppet::ast::attribute_expression()> attribute_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::attribute_operator()> attribute_operator;
        boost::spirit::qi::rule<iterator_type, puppet::ast::name()> attribute_name;
        boost::spirit::qi::rule<iterator_type, puppet::ast::resource_defaults_expression()> resource_defaults_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::resource_override_expression()> resource_override_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::class_definition_expression()> class_definition_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::defined_type_expression()> defined_type_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::node_definition_expression()> node_definition_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::hostname()> hostname;
        boost::spirit::qi::rule<iterator_type, puppet::ast::collection_expression()> collection_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::binary_query_expression()> binary_query_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::binary_query_operator()> binary_query_operator;
        boost::spirit::qi::rule<iterator_type, puppet::ast::query()> query;
        boost::spirit::qi::rule<iterator_type, puppet::ast::attribute_query_operator()> attribute_query_operator;
        boost::spirit::qi::rule<iterator_type, puppet::ast::basic_expression()> attribute_query_value;

        // Unary expressions
        boost::spirit::qi::rule<iterator_type, puppet::ast::unary_expression()> unary_expression;

        // Postfix expressions
        boost::spirit::qi::rule<iterator_type, puppet::ast::postfix_subexpression()> postfix_subexpression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::selector_expression()> selector_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::selector_case_expression()> selector_case_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::access_expression()> access_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::method_call_expression()> method_call_expression;

        // Binary expressions
        boost::spirit::qi::rule<iterator_type, puppet::ast::binary_expression()> binary_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::binary_operator()> binary_operator;

        // Type expression
        boost::spirit::qi::rule<iterator_type, puppet::ast::primary_expression()> type_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::primary_expression()> variable_type_expression;
        boost::spirit::qi::rule<iterator_type, puppet::ast::postfix_subexpression()> type_access_expression;
    };

}}  // namespace puppet::compiler

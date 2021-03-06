/**
 * @file
 * Declares the AST resource override expression.
 */
#pragma once

#include "resource_expression.hpp"
#include <boost/optional.hpp>
#include <vector>
#include <iostream>

namespace puppet { namespace ast {

    /**
     * Represents an AST resource override expression.
     */
    struct resource_override_expression
    {
        /**
         * Default constructor for resource_override_expression.
         */
        resource_override_expression();

        /**
         * Constructs a resource override expression with the given reference expression and optional attributes to override.
         * @param reference The reference expression for resources being overriden.
         * @param attributes The optional list of attributes being overridden.
         */
        resource_override_expression(primary_expression reference, boost::optional<std::vector<attribute_expression>> attributes);

        /**
         * Gets the reference expression for the resources being overriden.
         * @return Returns the reference expression for the resources being overriden.
         */
        primary_expression const& reference() const;

        /**
         * Gets the optional attributes being overridden.
         * @return Returns the attributes being overridden.
         */
        boost::optional<std::vector<attribute_expression>> const& attributes() const;

        /**
         * Gets the position of the resource override expression.
         * @return Returns the position of the resource override expression.
         */
        lexer::position const& position() const;

     private:
        primary_expression _reference;
        boost::optional<std::vector<attribute_expression>> _attributes;
    };

    /**
     * Stream insertion operator for AST resource overide expression.
     * @param os The output stream to write the resource override expression to.
     * @param expr The resource override expression to write.
     * @return Returns the given output stream.
     */
    std::ostream& operator<<(std::ostream& os, resource_override_expression const& expr);

}}  // namespace puppet::ast

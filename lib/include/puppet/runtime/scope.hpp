/**
 * @file
 * Declares the runtime scope.
 */
#pragma once

#include "values/value.hpp"
#include "../facts/provider.hpp"
#include <unordered_map>
#include <string>
#include <memory>
#include <cstdint>

namespace puppet { namespace runtime {

    // Forward declaration of resource.
    struct resource;

    /**
     * Represents an assigned variable.
     */
    struct assigned_variable
    {
        /**
         * Constructs an assigned variable with the given value and location.
         * @param value The value of the variable.
         * @param path The path of the file where the variable was assigned.
         * @param line The line where the variable was assigned.
         */
        assigned_variable(std::shared_ptr<values::value const> value, std::shared_ptr<std::string> path = nullptr, size_t line = 0);

        /**
         * Gets the value of the variable.
         * @return Returns the value of the variable.
         */
        std::shared_ptr<values::value const> const& value() const;

        /**
         * Gets the path of the file where the variable was assigned.
         * @return Returns the path of the file where the variable was assigned.
         */
        std::string const* path() const;

        /**
         * Gets the line where the variable was assigned.
         * @return Returns the line where the variable was assigned.
         */
        size_t line() const;

     private:
        std::shared_ptr<values::value const> _value;
        std::shared_ptr<std::string> _path;
        size_t _line;
    };

    /**
     * Represents a runtime scope.
     */
    struct scope
    {
        /**
         * Constructs a scope.
         * @param parent The parent scope.
         * @param resource The resource associated with the scope.
         */
        explicit scope(std::shared_ptr<scope> parent, runtime::resource* resource = nullptr);

        /**
         * Constructs the top scope.
         * @param facts The facts provider to use for the top scope.
         * @param resource The resource associated with the top scope.
         */
        explicit scope(std::shared_ptr<facts::provider> facts, runtime::resource* resource = nullptr);

        /**
         * Gets the parent scope.
         * @return Returns the parent scope or nullptr if at top scope.
         */
        std::shared_ptr<scope> const& parent() const;

        /**
         * Gets the resource associated with the scope.
         * Resources associated with a scope denote the container resource.
         * @return Returns the resource associated with the scope or nullptr if there is no associated resource.
         */
        runtime::resource* resource() const;

        /**
         * Qualifies the given name using the scope's name.
         * @param name The name to qualify.
         * @return Returns the fully-qualified name.
         */
        std::string qualify(std::string const& name) const;

        /**
         * Sets a variable in the scope.
         * @param name The name of the variable.
         * @param value The value of the variable.
         * @param path The path of the file where the variable is being assigned or nullptr if unknown.
         * @param line The line number where the variable is being assigned or 0 if unknown.
         * @return Returns nullptr if the set was successful or a pointer to the previously assigned variable if there is already a variable of the same name.
         */
        assigned_variable const* set(std::string name, std::shared_ptr<values::value const> value, std::shared_ptr<std::string> path = nullptr, size_t line = 0);

        /**
         * Gets a variable in the scope.
         * @param name The name of the variable to get.
         * @return Returns the assigned variable or nullptr if the variable does not exist in the scope.
         */
        assigned_variable const* get(std::string const& name);

     private:
        std::shared_ptr<facts::provider> _facts;
        std::shared_ptr<scope> _parent;
        runtime::resource* _resource;
        std::unordered_map<std::string, assigned_variable> _variables;
    };

    /**
     * Stream insertion operator for runtime scope.
     * @param os The output stream to write the runtime scope to.
     * @param s The runtime scope to write.
     * @return Returns the given output stream.
     */
    std::ostream& operator<<(std::ostream& os, scope const& s);

}}  // puppet::runtime

/**
 * @file
 * Declares the equals operator.
 */
#pragma once

#include "binary_context.hpp"

namespace puppet { namespace runtime { namespace operators {

    /**
     * Implements the equals operator.
     */
    struct equals
    {
        /**
         * Called to invoke the operator.
         * @param context The operator context.
         * @return Returns the resulting value.
         */
        values::value operator()(binary_context& context) const;
    };

}}}  // puppet::runtime::operators

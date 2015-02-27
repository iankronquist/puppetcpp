set(LIBPUPPET_VERSION_MAJOR 0)
set(LIBPUPPET_VERSION_MINOR 1)
set(LIBPUPPET_VERSION_PATCH 0)

include_directories(
    include/
    ${Boost_INCLUDE_DIRS}
)

# Add the executable for generating the static lexer
add_executable(generate_static_lexer src/lexer/generate_static_lexer.cc)
target_link_libraries(generate_static_lexer ${Boost_LIBRARIES})

# Add the command to generate the static lexer
add_custom_command(COMMAND generate_static_lexer WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/lexer OUTPUT ${CMAKE_SOURCE_DIR}/include/puppet/lexer/static_lexer.hpp DEPENDS include/puppet/lexer/lexer.hpp)

add_library(puppet SHARED
    include/puppet/lexer/static_lexer.hpp
    src/lexer/lexer.cc
    src/lexer/string_token.cc
    src/lexer/token_id.cc
    src/ast/access_expression.cc
    src/ast/array.cc
    src/ast/boolean.cc
    src/ast/case_expression.cc
    src/ast/class_definition_expression.cc
    src/ast/collection_expression.cc
    src/ast/defined_type_expression.cc
    src/ast/expression.cc
    src/ast/function_call_expression.cc
    src/ast/hash.cc
    src/ast/if_expression.cc
    src/ast/lambda.cc
    src/ast/manifest.cc
    src/ast/method_call_expression.cc
    src/ast/name.cc
    src/ast/node_definition_expression.cc
    src/ast/number.cc
    src/ast/parameter.cc
    src/ast/regex.cc
    src/ast/selector_expression.cc
    src/ast/undef.cc
    src/ast/resource_defaults_expression.cc
    src/ast/resource_expression.cc
    src/ast/resource_override_expression.cc
    src/ast/string.cc
    src/ast/type.cc
    src/ast/unless_expression.cc
    src/ast/variable.cc
    src/ast/visitors.cc
    src/parser/parser.cc
    src/utility/error_reporter.cc
)

set_target_properties(puppet PROPERTIES VERSION "${LIBPUPPET_VERSION_MAJOR}.${LIBPUPPET_VERSION_MINOR}.${LIBPUPPET_VERSION_PATCH}" COTIRE_UNITY_LINK_LIBRARIES_INIT "COPY_UNITY")

target_link_libraries(puppet
    ${Boost_LIBRARIES}
)

install(TARGETS puppet DESTINATION lib)

cotire(puppet)

add_subdirectory(tests)
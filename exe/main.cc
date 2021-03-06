#include <puppet/compiler/settings.hpp>
#include <puppet/compiler/node.hpp>
#include <puppet/facts/yaml.hpp>

using namespace std;
using namespace puppet::logging;
using namespace puppet::facts;
namespace compiler = puppet::compiler;

int main(int argc, char const* argv[])
{
    console_logger logger;

    try {
        compiler::settings settings(argc, argv);

        if (settings.show_version()) {
            cout << "0.1.0-FIXME" << endl;
            return EXIT_SUCCESS;
        }
        if (settings.show_help()) {
            settings.print_usage();
            return EXIT_SUCCESS;
        }

        logger.level(settings.log_level());

        // Log some useful information for debugging purposes
        LOG(debug, "using directory '%1%' as the code directory.", settings.code_directory());
        LOG(debug, "using directory '%1%' as the environment directory.", settings.environment_directory());
        for (auto const& directory : settings.module_directories()) {
            LOG(debug, "using directory '%1%' to search for global modules.", directory);
        }

        // Construct an environment
        compiler::environment environment(settings.environment(), settings.environment_directory());

        // Construct a node
        compiler::node node(settings.node_name(), environment);

        // TODO: remove this check
        if (settings.manifests().empty()) {
            throw compiler::settings_exception("expected at least one manifest to compile (default manifest file not yet implemented).");
        }

        try {
            LOG(notice, "compiling for node '%1%' with environment '%2%'.", settings.node_name(), settings.environment());

            // Compile the manifest
            auto catalog = node.compile(logger, settings);

            // TODO: output the catalog
        } catch (compiler::compilation_exception const& ex) {
            LOG(error, ex.line(), ex.column(), ex.text(), ex.path(), "node '%1%': %2%", node.name(), ex.what());
        }
    } catch (yaml_parse_exception const& ex) {
        LOG(error, ex.line(), ex.column(), ex.text(), ex.path(), ex.what());
    } catch (compiler::settings_exception const& ex) {
        LOG(error, "%1%", ex.what());
        LOG(notice, "use 'puppetcpp --help' for help.");
        return EXIT_FAILURE;
    }
    catch (exception const& ex) {
        LOG(critical, "unhandled exception: %1%", ex.what());
    }

    auto errors = logger.errors();
    auto warnings = logger.warnings();

    LOG(notice, "compilation %1% with %2% %3% and %4% %5%.",
        (errors > 0 ? "failed" : "succeeded"),
        errors,
        (errors != 1 ? "errors" : "error"),
        warnings,
        (warnings != 1 ? "warnings" : "warning")
    );
    return errors > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

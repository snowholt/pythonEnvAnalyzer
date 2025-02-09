project('python-dep-analyzer', 'c',
    version: '0.1.0',
    license: 'MIT',
    default_options: [
        'c_std=c11',
        'warning_level=3',
        'buildtype=debugoptimized',
    ],
)

# Dependencies
gtk_dep = dependency('gtk4')
json_dep = dependency('json-glib-1.0')
graphviz_dep = dependency('libgvc')
sqlite_dep = dependency('sqlite3')
graphene_dep = dependency('graphene-1.0')

# Source files
src_files = files(
    'src/main.c',
    'src/core/analyzer.c',
    'src/core/package.c',  # Make sure this line exists
    'src/db/database.c',
    'src/ui/main_window.c',
    'src/ui/graph_view.c',
    'src/ui/package_list.c',
)

# Include directories
inc = include_directories(
    'src',
    'include',
    'src/core',
)

# Add pkg-config dependencies paths
pkg_deps = [gtk_dep, json_dep, graphviz_dep, sqlite_dep]
foreach dep : pkg_deps
    inc_dirs = dep.get_variable(pkgconfig: 'includedir', internal: 'include_directories')
    if inc_dirs != ''
        inc = [inc, include_directories(inc_dirs)]
    endif
endforeach

# Resources
gnome = import('gnome')
resources = gnome.compile_resources(
    'resources',
    'resources/resources.gresource.xml',
    source_dir: 'resources',
)

# Executable
executable('python-dep-analyzer',
    src_files,
    resources,
    dependencies: [
        gtk_dep,
        json_dep,
        graphviz_dep,
        sqlite_dep,
        graphene_dep,
    ],
    include_directories: inc,
    install: true,
)

# Installation paths
install_data('resources/icons/venv-analyzer.svg',
    install_dir: get_option('datadir') / 'icons/hicolor/scalable/apps',
    install_tag: 'runtime'
)

install_data('resources/venv-analyzer.desktop',
    install_dir: get_option('datadir') / 'applications',
    install_tag: 'runtime'
)

# Tests setup
if get_option('tests').enabled()
    test_deps = [
        gtk_dep,
        json_dep,
        graphviz_dep,
        sqlite_dep,
    ]
    
    test_src = files(
        'tests/test_package.c',
        'tests/test_analyzer.c',
        'tests/test_database.c',
    )
    
    test_exe = executable('run-tests',
        test_src,
        dependencies: test_deps,
        include_directories: inc,
    )
    
    test('unit tests', test_exe)
endif

# Configuration summary
summary({
    'prefix': get_option('prefix'),
    'bindir': get_option('bindir'),
    'datadir': get_option('datadir'),
    'Tests': get_option('tests').enabled(),
}, section: 'Paths')

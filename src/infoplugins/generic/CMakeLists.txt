include_directories(${LIBECHONEST_INCLUDE_DIR})

list(APPEND simple_plugins
    Echonest
    Charts
    NewReleases
    Spotify
    Hypem
    MusixMatch
    MusicBrainz
    Rovi
)

foreach(simple_plugin ${simple_plugins})
    STRING(TOLOWER "${simple_plugin}" dir)
    add_tomahawk_plugin(${dir}
        TYPE infoplugin EXPORT_MACRO INFOPLUGINDLLEXPORT_PRO
        SOURCES "${dir}/${simple_plugin}Plugin.cpp"
    )
endforeach()


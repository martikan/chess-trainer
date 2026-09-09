install(FILES io.github.martikan.ChessTrainer.desktop
        DESTINATION ${KDE_INSTALL_APPDIR})

install(FILES io.github.martikan.ChessTrainer.metainfo.xml
        DESTINATION ${KDE_INSTALL_METAINFODIR})

# The application icon is a raster, so it ships as a size set rather than a
# single scalable file: icon themes only look inside scalable/ for SVGs, and a
# PNG placed there would never be found. Pre-scaled sizes also render better at
# panel and menu sizes than letting the loader downscale 256x256 on the fly.
foreach(size 16 24 32 48 64 128 256)
    install(FILES icons/hicolor/${size}x${size}/apps/io.github.martikan.ChessTrainer.png
            DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/${size}x${size}/apps)
endforeach()

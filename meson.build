project('mediaplayer', 'c')
gnome = import('gnome')

src = ['main.c', 'mediaplayer-app.c', 'mediaplayer-window.c']
deps = [
    dependency('gtk+-3.0'),
    dependency('gstreamer-1.0'),
    dependency('gstreamer-video-1.0')
]

resources = gnome.compile_resources('mediaplayer-resources',
                                    'mediaplayer.gresource.xml',
                                    source_dir: '.')

executable('Mediaplayer',
           src, resources,
           dependencies: deps)

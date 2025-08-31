output_plugins_sources = [
  'NullOutputPlugin.cxx',
]

output_plugins_deps = [
  output_api_dep,
  config_dep,
  tag_dep,
  log_dep,
]

need_encoder = false
need_wave_encoder = false

# All output plugins disabled for mpd-dbcreate - only NullOutputPlugin needed
# Set all feature flags to false
output_features.set('ENABLE_AO', false)
output_features.set('HAVE_FIFO', false)
output_features.set('ENABLE_HTTPD_OUTPUT', false)
output_features.set('ENABLE_JACK', false)
output_features.set('HAVE_OPENAL', false)
output_features.set('HAVE_OSX', false)
output_features.set('ENABLE_PIPE_OUTPUT', false)
output_features.set('ENABLE_RECORDER_OUTPUT', false)
output_features.set('HAVE_SHOUT', false)
output_features.set('ENABLE_SNAPCAST_OUTPUT', false)
output_features.set('ENABLE_SOLARIS_OUTPUT', false)
output_features.set('ENABLE_WINMM_OUTPUT', false)
output_features.set('ENABLE_WASAPI_OUTPUT', false)

# Define empty dependencies to avoid build errors
libao_dep = dependency('', required: false)
libjack_dep = dependency('', required: false)
openal_dep = dependency('', required: false)
libshout_dep = dependency('', required: false)
sles_dep = dependency('', required: false)
winmm_dep = dependency('', required: false)
wasapi_dep = dependency('', required: false)

output_plugins = static_library(
  'output_plugins',
  output_plugins_sources,
  include_directories: inc,
  dependencies: output_plugins_deps,
)

output_plugins_dep = declare_dependency(
  link_with: output_plugins,
  dependencies: output_plugins_deps,
)

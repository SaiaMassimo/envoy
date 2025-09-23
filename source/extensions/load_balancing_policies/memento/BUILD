load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_extension",
    "envoy_cc_library",
    "envoy_extension_package",
)

licenses(["notice"])  # Assumendo licenza Apache 2.0 come il resto di Envoy

envoy_extension_package()

# 1. Libreria per la logica core dell'algoritmo Memento (nella sottodirectory cpuls)
# Questa libreria non dipende da altre parti di Envoy.
envoy_cc_library(
    name = "memento_core_lib",
    srcs = [
        "cpuls/BinomialEngine.cc",
        "cpuls/MementoWithBinomial.cc",
        "cpuls/MementoWithBinomialEngine.cpp", # Attenzione all'estensione .cpp
    ],
    hdrs = [
        "cpuls/BinomialEngine.h",
        "cpuls/MementoWithBinomial.h",
        "cpuls/MementoWithBinomialEngine.h",
    ],
    # Nessuna dipendenza da Envoy qui, il che è un ottimo design
    deps = [],
)

# 2. Libreria per l'integrazione del LB in Envoy
# Questa libreria collega la logica core con le interfacce di Envoy.
envoy_cc_library(
    name = "memento_lb_lib",
    srcs = [
        "memento_lb.cc",
        "memento_table.cc",
    ],
    hdrs = [
        "memento_lb.h",
        "memento_table.h",
    ],
    deps = [
        ":memento_core_lib",  # Dipende dalla nostra logica core
        "//source/common/upstream:load_balancer_lib",
        "//source/extensions/load_balancing_policies/common:thread_aware_lb_lib",
        "@envoy_api//envoy/config/cluster/v3:pkg_cc_proto",
    ],
)

# 3. Configurazione dell'estensione che registra la factory
# Questo è il punto di ingresso che Envoy usa per trovare il tuo LB.
envoy_cc_extension(
    name = "config",
    srcs = ["memento_lb_factory.cc"],
    # Non ci sono header specifici per la factory, quindi hdrs è vuoto
    deps = [
        ":memento_lb_lib", # Dipende dalla libreria di integrazione
        "//source/extensions/load_balancing_policies/common:factory_base",
        # Dipendenza dal file .proto generato per la configurazione
        "@envoy_api//envoy/extensions/load_balancing_policies/memento/v3:pkg_cc_proto",
    ],
)
simics_add_module(sample-risc
    CLASSES sample-risc sample-risc-core
    SOURCES sample-risc.c sample-risc-core.c sample-risc-queue.c sample-risc-frequency.c sample-risc-step.c sample-risc-cycle.c sample-risc-memory.c sample-risc-exec.c event-queue.c toy-risc-core.c sample-risc-direct-memory.c module_load.py
    SIMICS_API 6
    INIT_LOCAL
)
target_link_libraries(sample-risc
    PRIVATE Simics::Core::cycle-common
)
add_compile_definitions(SAMPLE_RISC_CPU_CLASSNAME=sample-risc SAMPLE_RISC_CORE_CLASSNAME=sample-risc-core )

add_library(sample-risc-shared STATIC
  event-queue.c)
target_include_directories(sample-risc-shared PRIVATE ${SIMICS_INC})
add_library(Simics::sample-risc ALIAS sample-risc-shared)

add_custom_command(
  OUTPUT module_load.py
  COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_CURRENT_SOURCE_DIR}/sample_risc_common.py ${CMAKE_CURRENT_SOURCE_DIR}/module_load_module.py > module_load.py
  DEPENDS sample_risc_common.py module_load_module.py
)

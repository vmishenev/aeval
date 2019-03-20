add_executable (aeval Ae.cpp)
target_link_libraries (aeval synthlib2parser ${Z3_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${GMPXX_LIB} ${GMP_LIB})
llvm_config (aeval bitwriter)
install(TARGETS aeval RUNTIME DESTINATION bin)

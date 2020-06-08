include(CheckSymbolExists)

if(__get_target_arch)
    return()
endif()
set(__get_target_arch YES)

function(get_target_arch _var_arch)
    check_symbol_exists(__x86_64__ "" ARCH_IS_X86_64)
    check_symbol_exists(__aarch64__ "" ARCH_IS_AARCH64)

    if(ARCH_IS_X86_64)
        set(${_var_arch} "x86_64" PARENT_SCOPE)
    elseif(ARCH_IS_AARCH64)
        set(${_var_arch} "aarch64" PARENT_SCOPE)
    else()
        set(${_var_arch} "unsupported" PARENT_SCOPE)
    endif()
endfunction()

//
// Created by nfrmtk on 6/14/25.
//
#ifndef ASSERT_HPP
#define ASSERT_HPP
#include <util/system/yassert.h>
#define PMERGE_ASSERT_M(condition, message) Y_DEBUG_ABORT_UNLESS(condition, message)
#define PMERGE_ASSERT(condition) Y_DEBUG_ABORT_UNLESS(condition)
#endif // ASSERT_HPP

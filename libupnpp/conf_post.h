/* Copyright (C) 2016 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/* Definitions for config.h which we don't want autoheader to clobber */

// Get rid of macro names which could conflict with other package's
#if defined(LIBUPNPP_NEED_PACKAGE_VERSION) && \
    !defined(LIBUPNPP_PACKAGE_VERSION_DEFINED)
#define LIBUPNPP_PACKAGE_VERSION_DEFINED
static const char *LIBUPNPP_PACKAGE_VERSION = PACKAGE_VERSION;
#endif
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_URL
#undef PACKAGE_VERSION

#ifdef __cplusplus
#ifdef  HAVE_CXX0X_UNORDERED
#  include <unordered_map>
#  include <unordered_set>
#  include <functional>
#  include <memory>
#  define STD_UNORDERED_MAP std::unordered_map
#  define STD_UNORDERED_SET std::unordered_set
#  define STD_FUNCTION      std::function
#  define STD_BIND          std::bind
#  define STD_PLACEHOLDERS  std::placeholders
#  define STD_SHARED_PTR    std::shared_ptr
#  define STD_WEAK_PTR      std::weak_ptr
#elif defined(HAVE_TR1_UNORDERED)
#  include <tr1/unordered_map>
#  include <tr1/unordered_set>
#  include <tr1/functional>
#  include <tr1/memory>
#  define STD_UNORDERED_MAP std::tr1::unordered_map
#  define STD_UNORDERED_SET std::tr1::unordered_set
#  define STD_FUNCTION      std::tr1::function
#  define STD_BIND          std::tr1::bind
#  define STD_PLACEHOLDERS  std::tr1::placeholders
#  define STD_SHARED_PTR    std::tr1::shared_ptr
#  define STD_WEAK_PTR      std::tr1::weak_ptr
#else
#  include <map>
#  include <set>
#  include <functional>
#  define STD_UNORDERED_MAP std::map
#  define STD_UNORDERED_SET std::set
/* Yeah we're cooked if the code uses these features */
#  define STD_FUNCTION 
#  define STD_BIND
#  define STD_PLACEHOLDERS
#  define STD_SHARED_PTR
#  define STD_WEAK_PTR  
#endif

#endif /* c++ */

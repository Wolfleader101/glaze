// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#endif

#include "glaze/core/common.hpp"

namespace glz
{
   namespace detail
   {
      template <class T>
      struct hostname_includer
      {
         T& value;

         static constexpr auto glaze_includer = true;
      };
   }

   template <class T>
   struct meta<detail::hostname_includer<T>>
   {
      static constexpr std::string_view name = detail::join_v<chars<"hostname_includer<">, name_v<T>, chars<">">>;
   };

   // Register this with an object to allow file including based on the hostname
   // This is useful for configuration files that should be specific to a host
   struct hostname_include final
   {
      bool reflection_helper{}; // needed for count_members
      static constexpr auto glaze_includer = true;
      static constexpr auto glaze_reflect = false;

      constexpr decltype(auto) operator()(auto&& value) const noexcept
      {
         using V = std::decay_t<decltype(value)>;
         return detail::hostname_includer<V>{value};
      }
   };

   namespace detail
   {
      inline void replace_first_braces(std::string& original, const std::string& replacement) noexcept
      {
         static constexpr std::string_view braces = "{}";

         if (size_t pos = original.find(braces); pos != std::string::npos) {
            original.replace(pos, braces.size(), replacement);
         }
      }

      inline std::string get_hostname(context& ctx) noexcept
      {
         char hostname[256]{};

#ifdef _WIN32
         WSADATA wsaData;
         if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            ctx.error = error_code::hostname_failure;
            return {};
         }
#endif

         if (gethostname(hostname, sizeof(hostname))) {
            ctx.error = error_code::hostname_failure;
            return {};
         }

#ifdef _WIN32
         WSACleanup();
#endif

         return {hostname};
      }

      template <class T>
      struct from_json<hostname_includer<T>>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            std::string& path = string_buffer();
            read<json>::op<Opts>(path, ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;

            replace_first_braces(path, get_hostname(ctx));
            if (bool(ctx.error)) [[unlikely]]
               return;

            const auto file_path = relativize_if_not_absolute(std::filesystem::path(ctx.current_file).parent_path(),
                                                              std::filesystem::path{path});

            std::string& buffer = path;
            const auto string_file_path = file_path.string();
            const auto ec = file_to_buffer(buffer, string_file_path);

            if (bool(ec)) [[unlikely]] {
               ctx.error = ec;
               return;
            }

            const auto current_file = ctx.current_file;
            ctx.current_file = string_file_path;

            std::ignore = glz::read<Opts>(value.value, buffer, ctx);
            if (bool(ctx.error)) [[unlikely]]
               return;

            ctx.current_file = current_file;
         }
      };

      template <class T>
      struct to_json<hostname_includer<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&&...) noexcept
         {}
      };
   }
}

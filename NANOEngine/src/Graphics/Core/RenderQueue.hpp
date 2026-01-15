#pragma once  
#include <cstdint>  

// To avoid macro conflicts with Windows headers
#ifdef TRANSPARENT
#undef TRANSPARENT
#endif

namespace NE::Graphics {  
   enum class RenderQueue : uint32_t {  
       BACKGROUND = 1000,  
       GEOMETRY = 2000,  
       ALPHATEST = 2450, 
       TRANSPARENT = 3000, 
       OVERLAY = 4000  
   };
}
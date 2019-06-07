#include "lp_context.h"
#include "lp_state.h"
#include "draw/draw_private.h"
#include "draw/draw_llvm.h"

static void llvmpipe_set_shader_buffers(struct pipe_context *pipe,
                                        enum pipe_shader_type shader,
                                        unsigned start,
                                        unsigned num,
                                        const struct pipe_shader_buffer *buffers,
                                        unsigned writable_bitmask)
{
   struct llvmpipe_context *llvmpipe = llvmpipe_context(pipe);
   unsigned i;
   assert(shader < PIPE_SHADER_TYPES);
   assert(start + num <= ARRAY_SIZE(llvmpipe->buffers[shader]));

   draw_flush(llvmpipe->draw);

   for (i = 0; i < num; i++) {
      int idx = start + i;
      if (buffers) {
         struct llvmpipe_resource *buffer = llvmpipe_resource(buffers[i].buffer);
         pipe_resource_reference(&llvmpipe->buffers[shader][idx].buffer, buffers[i].buffer);
         llvmpipe->buffers[shader][idx] = buffers[i];

         if (shader == PIPE_SHADER_VERTEX/* || shader == PIPE_SHADER_GEOMETRY*/) {
            draw_set_buffer(llvmpipe->draw, shader, idx, buffer->data);
         } else if (shader == PIPE_SHADER_FRAGMENT)
         {
            llvmpipe->dirty |= LP_NEW_SSBO;
         }
      }
      else {
         pipe_resource_reference(&llvmpipe->buffers[shader][idx].buffer, NULL);
         memset(&llvmpipe->buffers[shader][idx], 0, sizeof(struct pipe_shader_buffer));

         if (shader == PIPE_SHADER_VERTEX/* || shader == PIPE_SHADER_GEOMETRY*/) {
            draw_set_buffer(llvmpipe->draw, shader, idx, NULL);
         }
      }
   }
}

void llvmpipe_init_image_funcs(struct llvmpipe_context *llvmpipe)
{
   //pipe->set_shader_images = softpipe_set_shader_images;
   llvmpipe->pipe.set_shader_buffers = llvmpipe_set_shader_buffers;
}

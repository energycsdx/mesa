/*
 * Copyright (C) 2019 Alyssa Rosenzweig
 * Copyright (C) 2014-2017 Broadcom
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "pan_context.h"
#include "util/hash_table.h"
#include "util/ralloc.h"
#include "util/u_format.h"
#include "util/u_pack_color.h"

struct panfrost_job *
panfrost_create_job(struct panfrost_context *ctx)
{
        struct panfrost_job *job = rzalloc(ctx, struct panfrost_job);

        job->ctx = ctx;

        job->bos = _mesa_set_create(job,
                                    _mesa_hash_pointer,
                                    _mesa_key_pointer_equal);

        job->minx = job->miny = ~0;
        job->maxx = job->maxy = 0;

        util_dynarray_init(&job->headers, job);
        util_dynarray_init(&job->gpu_headers, job);
 
        return job;
}

void
panfrost_free_job(struct panfrost_context *ctx, struct panfrost_job *job)
{
        if (!job)
                return;

        set_foreach(job->bos, entry) {
                struct panfrost_bo *bo = (struct panfrost_bo *)entry->key;
                panfrost_bo_unreference(ctx->base.screen, bo);
        }

        _mesa_hash_table_remove_key(ctx->jobs, &job->key);

        if (ctx->job == job)
                ctx->job = NULL;

        ralloc_free(job);
}

struct panfrost_job *
panfrost_get_job(struct panfrost_context *ctx,
                struct pipe_surface **cbufs, struct pipe_surface *zsbuf)
{
        /* Lookup the job first */

        struct panfrost_job_key key = {
                .cbufs = {
                        cbufs[0],
                        cbufs[1],
                        cbufs[2],
                        cbufs[3],
                },
                .zsbuf = zsbuf
        };
        
        struct hash_entry *entry = _mesa_hash_table_search(ctx->jobs, &key);

        if (entry)
                return entry->data;

        /* Otherwise, let's create a job */

        struct panfrost_job *job = panfrost_create_job(ctx);

        /* Save the created job */

        memcpy(&job->key, &key, sizeof(key));
        _mesa_hash_table_insert(ctx->jobs, &job->key, job);

        return job;
}

/* Get the job corresponding to the FBO we're currently rendering into */

struct panfrost_job *
panfrost_get_job_for_fbo(struct panfrost_context *ctx)
{
        /* If we're wallpapering, we special case to workaround
         * u_blitter abuse */

        if (ctx->wallpaper_batch)
                return ctx->wallpaper_batch;

        /* If we already began rendering, use that */

        if (ctx->job)
                return ctx->job;

        /* If not, look up the job */

        struct pipe_surface **cbufs = ctx->pipe_framebuffer.cbufs;
        struct pipe_surface *zsbuf = ctx->pipe_framebuffer.zsbuf;
        struct panfrost_job *job = panfrost_get_job(ctx, cbufs, zsbuf);

        return job;
}

void
panfrost_job_add_bo(struct panfrost_job *job, struct panfrost_bo *bo)
{
        if (!bo)
                return;

        if (_mesa_set_search(job->bos, bo))
                return;

        panfrost_bo_reference(bo);
        _mesa_set_add(job->bos, bo);
}

void
panfrost_flush_jobs_writing_resource(struct panfrost_context *panfrost,
                                struct pipe_resource *prsc)
{
#if 0
        struct hash_entry *entry = _mesa_hash_table_search(panfrost->write_jobs,
                                                           prsc);
        if (entry) {
                struct panfrost_job *job = entry->data;
                panfrost_job_submit(panfrost, job);
        }
#endif
        /* TODO stub */
}

void
panfrost_job_submit(struct panfrost_context *ctx, struct panfrost_job *job)
{
        int ret;

        panfrost_scoreboard_link_batch(job);

        bool has_draws = job->last_job.gpu;
        bool is_scanout = panfrost_is_scanout(ctx);

        if (!job)
                return;

        ret = panfrost_drm_submit_vs_fs_job(ctx, has_draws, is_scanout);

        if (ret)
                fprintf(stderr, "panfrost_job_submit failed: %d\n", ret);
}

void
panfrost_job_set_requirements(struct panfrost_context *ctx,
                         struct panfrost_job *job)
{
        if (ctx->rasterizer && ctx->rasterizer->base.multisample)
                job->requirements |= PAN_REQ_MSAA;

        if (ctx->depth_stencil && ctx->depth_stencil->depth.writemask)
                job->requirements |= PAN_REQ_DEPTH_WRITE;
}

static uint32_t
pan_pack_color(const union pipe_color_union *color, enum pipe_format format)
{
        /* Alpha magicked to 1.0 if there is no alpha */

        bool has_alpha = util_format_has_alpha(format);
        float clear_alpha = has_alpha ? color->f[3] : 1.0f;

        /* Packed color depends on the framebuffer format */

        const struct util_format_description *desc =
                util_format_description(format);

        if (util_format_is_rgba8_variant(desc)) {
                return (float_to_ubyte(clear_alpha) << 24) |
                       (float_to_ubyte(color->f[2]) << 16) |
                       (float_to_ubyte(color->f[1]) <<  8) |
                       (float_to_ubyte(color->f[0]) <<  0);
        } else if (format == PIPE_FORMAT_B5G6R5_UNORM) {
                /* First, we convert the components to R5, G6, B5 separately */
                unsigned r5 = CLAMP(color->f[0], 0.0, 1.0) * 31.0;
                unsigned g6 = CLAMP(color->f[1], 0.0, 1.0) * 63.0;
                unsigned b5 = CLAMP(color->f[2], 0.0, 1.0) * 31.0;

                /* Then we pack into a sparse u32. TODO: Why these shifts? */
                return (b5 << 25) | (g6 << 14) | (r5 << 5);
        } else {
                /* Try Gallium's generic default path. Doesn't work for all
                 * formats but it's a good guess. */

                union util_color out;
                util_pack_color(color->f, format, &out);
                return out.ui[0];
        }

        return 0;
}

void
panfrost_job_clear(struct panfrost_context *ctx,
                struct panfrost_job *job,
                unsigned buffers,
                const union pipe_color_union *color,
                double depth, unsigned stencil)

{
        if (buffers & PIPE_CLEAR_COLOR) {
                enum pipe_format format = ctx->pipe_framebuffer.cbufs[0]->format;
                job->clear_color = pan_pack_color(color, format);
        }

        if (buffers & PIPE_CLEAR_DEPTH) {
                job->clear_depth = depth;
        }

        if (buffers & PIPE_CLEAR_STENCIL) {
                job->clear_stencil = stencil;
        }

        job->clear |= buffers;

        /* Clearing affects the entire framebuffer (by definition -- this is
         * the Gallium clear callback, which clears the whole framebuffer. If
         * the scissor test were enabled from the GL side, the state tracker
         * would emit a quad instead and we wouldn't go down this code path) */

        panfrost_job_union_scissor(job, 0, 0,
                        ctx->pipe_framebuffer.width,
                        ctx->pipe_framebuffer.height);
}

void
panfrost_flush_jobs_reading_resource(struct panfrost_context *panfrost,
                                struct pipe_resource *prsc)
{
        struct panfrost_resource *rsc = pan_resource(prsc);

        panfrost_flush_jobs_writing_resource(panfrost, prsc);

        hash_table_foreach(panfrost->jobs, entry) {
                struct panfrost_job *job = entry->data;

                if (_mesa_set_search(job->bos, rsc->bo)) {
                        printf("TODO: submit job for flush\n");
                        //panfrost_job_submit(panfrost, job);
                        continue;
                }
        }
}

static bool
panfrost_job_compare(const void *a, const void *b)
{
        return memcmp(a, b, sizeof(struct panfrost_job_key)) == 0;
}

static uint32_t
panfrost_job_hash(const void *key)
{
        return _mesa_hash_data(key, sizeof(struct panfrost_job_key));
}

/* Given a new bounding rectangle (scissor), let the job cover the union of the
 * new and old bounding rectangles */

void
panfrost_job_union_scissor(struct panfrost_job *job,
                unsigned minx, unsigned miny,
                unsigned maxx, unsigned maxy)
{
        job->minx = MIN2(job->minx, minx);
        job->miny = MIN2(job->miny, miny);
        job->maxx = MAX2(job->maxx, maxx);
        job->maxy = MAX2(job->maxy, maxy);
}

void
panfrost_job_init(struct panfrost_context *ctx)
{
        ctx->jobs = _mesa_hash_table_create(ctx,
                                            panfrost_job_hash,
                                            panfrost_job_compare);

        ctx->write_jobs = _mesa_hash_table_create(ctx,
                                            _mesa_hash_pointer,
                                            _mesa_key_pointer_equal);
}

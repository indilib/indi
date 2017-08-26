/*
 * Copyright (c) 2016, Ilia Platone, All rights reserved.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __LIBRTK_H
#define __LIBRTK_H


#ifdef _WIN32
#define DLL_EXPORT __declspec(dllexport)
#else
#define _POSIX_C_SOURCE 2
#define DLL_EXPORT extern 
#endif

DLL_EXPORT char * librtk_start (char **args, int narg, char *ret);
DLL_EXPORT char * librtk_stop (char **args, int narg, char *ret);
DLL_EXPORT char * librtk_restart (char **args, int narg, char *ret);
DLL_EXPORT char * librtk_solution (char **args, int narg, char *ret);
DLL_EXPORT char * librtk_status (char **args, int narg, char *ret);
DLL_EXPORT char * librtk_satellite (char **args, int narg, char *ret);
DLL_EXPORT char * librtk_observ (char **args, int narg, char *ret);
DLL_EXPORT char * librtk_navidata (char **args, int narg, char *ret);
DLL_EXPORT char * librtk_stream (char **args, int narg, char *ret);
DLL_EXPORT char * librtk_error (char **args, int narg, char *ret);
DLL_EXPORT char * librtk_option (char **args, int narg, char *ret);
DLL_EXPORT char * librtk_set (char **args, int narg, char *ret);
DLL_EXPORT char * librtk_load (char **args, int narg, char *ret);
DLL_EXPORT char * librtk_save (char **args, int narg, char *ret);
DLL_EXPORT char * librtk_log (char **args, int narg, char *ret);
DLL_EXPORT char * librtk_help (char **args, int narg, char *ret);
DLL_EXPORT int librtk_init (int argc, char **argv);
DLL_EXPORT void librtk_exit();
#endif

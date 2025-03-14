#include "rpihub75.h"

/**
 * @brief pass this function to your pthread_create() call to render a video file
 * will render the video file pointed to by scene->shader_file until
 * scene->do_render is false;
 * 
 * @param arg 
 * @return void* 
 */
void* render_video_fn(void *arg);

/**
 * @brief pass this function to your pthread_create() call to render a video file
 * will render the video file pointed to by scene->shader_file until
 * scene->do_render is false; returns once the video is done rendering
 * 
 * @param arg 
 * @return void* 
 */
bool hub_render_video(scene_info *scene, const char *filename);

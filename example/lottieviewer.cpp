/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <Elementary.h>
#include "lottieview.h"
#include "evasapp.h"
#include<iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <error.h>
#include <algorithm>

using namespace std;

typedef struct _AppInfo AppInfo;
struct _AppInfo {
   LottieView *view;
   Evas_Object *layout;
   Evas_Object *slider;
   Evas_Object *button;
   Ecore_Animator *animator;
   Eina_Bool autoPlaying;
};

typedef struct _ItemData ItemData;
struct _ItemData {
   int index;
};


std::vector<std::string> jsonFiles;
bool renderMode = true;

static void
_layout_del_cb(void *data, Evas *, Evas_Object *, void *)
{
   AppInfo *info = (AppInfo *)data;
   if (info->view) delete info->view;
   ecore_animator_del(info->animator);
   free(info);
}

static void
_update_frame_info(AppInfo *info)
{
   long currFrameNo = info->view->mCurFrame;
   long totalFrameNo = info->view->getTotalFrame();

   char buf[64];
   sprintf(buf, "%ld (total: %ld)", currFrameNo, totalFrameNo);
   elm_object_part_text_set(info->layout, "text", buf);
}

static void
_toggle_start_button(AppInfo *info)
{
   if (!info->autoPlaying)
     {
        info->autoPlaying = EINA_TRUE;
        info->view->play();
        elm_object_text_set(info->button, "Stop");
     }
   else
     {
        info->autoPlaying = EINA_FALSE;
        info->view->stop();
        elm_object_text_set(info->button, "Start");
     }
}

static Eina_Bool
_animator_cb(void *data)
{
    AppInfo *info = (AppInfo *)data;

    if (info && info->autoPlaying && info->view)
      {
         float pos = info->view->getPos();
         _update_frame_info(info);
         elm_slider_value_set(info->slider, (double)pos);
         evas_object_image_pixels_dirty_set(info->view->getImage(), EINA_TRUE);
         if (pos >= 1.0)
           _toggle_start_button(info);
      }
    return ECORE_CALLBACK_RENEW;
}

static void
_slider_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   double val = elm_slider_value_get(obj);
   AppInfo *info = (AppInfo *)data;

   if (!info->autoPlaying)
     {
        info->view->seek(val);
        evas_object_image_pixels_dirty_set(info->view->getImage(), EINA_TRUE);
     }

   _update_frame_info(info);
}

static void
_button_clicked_cb(void *data, Evas_Object */*obj*/, void */*event_info*/)
{
   AppInfo *info = (AppInfo *)data;
   if (info->view->getPos() >= 1.0f) info->view->mPos = 0.0f;
   _toggle_start_button(info);
}

Evas_Object *
create_layout(Evas_Object *parent, const char *file)
{
   Evas_Object *layout, *slider, *image, *button;
   Ecore_Animator *animator;
   AppInfo *info = (AppInfo *)calloc(sizeof(AppInfo), 1);

   //LAYOUT
   layout = elm_layout_add(parent);
   evas_object_show(layout);
   evas_object_size_hint_weight_set(layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

   std::string edjPath = DEMO_DIR;
   edjPath +="layout.edj";

   elm_layout_file_set(layout, edjPath.c_str(), "layout");

   //LOTTIEVIEW
   LottieView *view = new LottieView(evas_object_evas_get(layout), Strategy::renderCppAsync);
   view->setFilePath(file);
   view->setSize(500, 500);

   //IMAGE from LOTTIEVIEW
   image = view->getImage();
   evas_object_show(image);
   elm_object_part_content_set(layout, "lottie", image);

   //SLIDER
   slider = elm_slider_add(layout);
   elm_object_part_content_set(layout, "slider", slider);
   elm_slider_step_set(slider, 0.01);
   evas_object_smart_callback_add(slider, "changed", _slider_cb, (void *)info);

   button = elm_button_add(layout);
   elm_object_text_set(button, "Start");
   elm_object_part_content_set(layout, "button", button);
   evas_object_smart_callback_add(button, "clicked", _button_clicked_cb, (void *)info);

   animator = ecore_animator_add(_animator_cb, info);

   info->view = view;
   info->layout = layout;
   info->slider = slider;
   info->button = button;
   info->animator = animator;
   evas_object_event_callback_add(layout, EVAS_CALLBACK_DEL, _layout_del_cb, (void *)info);

   view->seek(0.0);
   _update_frame_info(info);

   return layout;
}

static void
_gl_selected_cb(void *data, Evas_Object */*obj*/, void *event_info)
{
   Evas_Object *nf = (Evas_Object *)data;
   Elm_Object_Item *it = (Elm_Object_Item *)event_info;
   elm_genlist_item_selected_set(it, EINA_FALSE);

   Evas_Object *layout = create_layout(nf, jsonFiles[elm_genlist_item_index_get(it) - 1].c_str());
   elm_naviframe_item_push(nf, NULL, NULL, NULL, layout, NULL);
}

static char *
_gl_text_get(void *data, Evas_Object */*obj*/, const char */*part*/)
{
   ItemData *id = (ItemData *) data;
   const char *ptr = strrchr(jsonFiles[id->index].c_str(), '/');
   int len = int(ptr + 1 - jsonFiles[id->index].c_str()); // +1 to include '/'
   return strdup(jsonFiles[id->index].substr(len).c_str());
}

static void
_gl_del(void */*data*/, Evas_Object */*obj*/)
{
}

EAPI_MAIN int
elm_main(int argc EINA_UNUSED, char **argv EINA_UNUSED)
{
   Evas_Object *win, *nf, *genlist;
   Elm_Genlist_Item_Class *itc = elm_genlist_item_class_new();
   ItemData *itemData;


   if (argc > 1) {
      if (!strcmp(argv[1], "--disable-render"))
         renderMode = false;
   }

   //WIN
   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);
   win = elm_win_util_standard_add("lottie", "LottieViewer");
   elm_win_autodel_set(win, EINA_TRUE);
   evas_object_resize(win, 500, 700);
   evas_object_show(win);

   //NAVIFRAME
   nf = elm_naviframe_add(win);
   evas_object_size_hint_weight_set(nf, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_win_resize_object_add(win, nf);
   evas_object_show(nf);

   //GENLIST
   genlist = elm_genlist_add(nf);
   elm_genlist_mode_set(genlist, ELM_LIST_COMPRESS);
   evas_object_smart_callback_add(genlist, "selected", _gl_selected_cb, nf);

   itc->item_style = "default";
   itc->func.text_get = _gl_text_get;
   itc->func.del = _gl_del;

   jsonFiles = EvasApp::jsonFiles(DEMO_DIR);

   for (uint32_t i = 0; i < jsonFiles.size(); i++) {
      itemData = (ItemData *)calloc(sizeof(ItemData), 1);
      itemData->index = i;
      elm_genlist_item_append(genlist, itc, (void *)itemData, NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);
   }

   elm_naviframe_item_push(nf, "Lottie Viewer", NULL, NULL, genlist, NULL);

   elm_run();

   return 0;
}
ELM_MAIN()

/* GStreamer
 * Copyright (C) 2009 Wim Taymans <wim.taymans@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <gst/gst.h>
#include <gio/gio.h>
/*
 * A simple RTP receiver 
 *
 *  receives alaw encoded RTP audio on port 5002, RTCP is received on  port 5003.
 *  the receiver RTCP reports are sent to port 5003
 *
 *             .-------.      .----------.     .---------.   .-------.   .--------.
 *  RTP        |udpsrc |      | rtpbin   |     |pcmadepay|   |alawdec|   |alsasink|
 *  port=5002  |      src->recv_rtp recv_rtp->sink     src->sink   src->sink      |
 *             '-------'      |          |     '---------'   '-------'   '--------'
 *                            |          |      
 *                            |          |     .-------.
 *                            |          |     |udpsink|  RTCP
 *                            |    send_rtcp->sink     | port=5003
 *             .-------.      |          |     '-------' sync=false
 *  RTCP       |udpsrc |      |          |               async=false
 *  port=5003  |     src->recv_rtcp      |                       
 *             '-------'      '----------'              
 */

/* the caps of the sender RTP stream. This is usually negotiated out of band with
 * SDP or RTSP. */
//#define AUDIO_CAPS "application/x-rtp,media=(string)audio,clock-rate=(int)8000,encoding-name=(string)AMR"
#define AUDIO_CAPS "application/x-rtp,media=(string)audio,clock-rate=(int)8000,encoding-name=(string)AMR,encoding-params=(string)1,octet-align=(string)1" 


#define GST_ON_SENDER_TIMEOUT   "on-sender-timeout"
#define GST_ON_TIMEOUT          "on-timeout"
#define GST_ON_BYE_SSRC         "on-bye-ssrc"
#define GST_ON_BYE_TIME_OUT     "on-bye-timeout"

/* the destination machine to send RTCP to. This is the address of the sender and
 * is used to send back the RTCP reports of this receiver. If the data is sent
 * from another machine, change this address. */
#define DEST_HOST "127.0.0.1"

/* print the stats of a source */
static void
print_source_stats (GObject * source)
{
  GstStructure *stats;
  gchar *str;

  if(source ==NULL)
  {
      printf("SOURCE ES NULL ------------------------------------------- \n");
      return;
  }
  
  
  /* get the source stats */
  g_object_get (source, "stats", &stats, NULL);

  /* simply dump the stats structure */
  str = gst_structure_to_string (stats);
  g_print ("source stats: %s\n", str);

  gst_structure_free (stats);
  g_free (str);
}

/* will be called when rtpbin signals on-ssrc-active. It means that an RTCP
 * packet was received from another source. */
static void
on_ssrc_active_cb (GstElement * rtpbin, guint sessid, guint ssrc,
    GstElement * depay)
{
  GObject *session, *isrc, *osrc;

  g_print ("got RTCP from session %u, SSRC %u\n", sessid, ssrc);

  /* get the right session */
  g_signal_emit_by_name (rtpbin, "get-internal-session", sessid, &session);

  /* get the internal source (the SSRC allocated to us, the receiver */
  g_object_get (session, "internal-source", &isrc, NULL);
  print_source_stats (isrc);

  /* get the remote source that sent us RTCP */
  g_signal_emit_by_name (session, "get-source-by-ssrc", ssrc, &osrc);
  print_source_stats (osrc);
}

/* will be called when rtpbin has validated a payload that we can depayload */
static void
pad_added_cb (GstElement * rtpbin, GstPad * new_pad, GstElement * depay)
{
  GstPad *sinkpad;
  GstPadLinkReturn lres;

  g_print ("new payload on pad: %s\n", GST_PAD_NAME (new_pad));

  sinkpad = gst_element_get_static_pad (depay, "sink");
  g_assert (sinkpad);

  lres = gst_pad_link (new_pad, sinkpad);
  g_assert (lres == GST_PAD_LINK_OK);
  gst_object_unref (sinkpad);
}


 GstElement *pipeline;
static void timeout_callback(GstElement* element, guint session, guint ssrc, gpointer user_data)
{
    if (user_data == NULL)
    	printf( "GST: Sending EOS TO THE pipeline !!!!\n");
    else 
        printf( "GST: Sending EOS TO THE pipeline in case:%s !!!!\n",(char *)user_data);
	gst_element_send_event(pipeline, gst_event_new_eos());
}

/* build a pipeline equivalent to:
 *
 * gst-launch-1.0 -v rtpbin name=rtpbin                                                \
 *      udpsrc caps=$AUDIO_CAPS port=5002 ! rtpbin.recv_rtp_sink_0              \
 *        rtpbin. ! rtppcmadepay ! alawdec ! audioconvert ! audioresample ! autoaudiosink \
 *      udpsrc port=5003 ! rtpbin.recv_rtcp_sink_0                              \
 *        rtpbin.send_rtcp_src_0 ! udpsink port=5003 host=$DEST sync=false async=false
 */
int
main (int argc, char *argv[])
{
  GstElement *rtpbin, *rtpsrc, *rtcpsrc, *rtcpsink;
  GstElement *audiodepay, *wavenc, *audiores, *audioconvert, *audiosink, *amrdecoder;

  GMainLoop *loop;
  GstCaps *caps;
  gboolean res;
  GstPadLinkReturn lres;
  GstPad *srcpad, *sinkpad;

  /* always init first */
  gst_init (&argc, &argv);

  /* the pipeline to hold everything */
  pipeline = gst_pipeline_new (NULL);
  g_assert (pipeline);

  /* the udp src and source we will use for RTP and RTCP */
  rtpsrc = gst_element_factory_make ("udpsrc", "rtpsrc");
  g_assert (rtpsrc);
  g_object_set (rtpsrc, "port", 5002, NULL);
  /* we need to set caps on the udpsrc for the RTP data */
  caps = gst_caps_from_string (AUDIO_CAPS);
  g_object_set (rtpsrc, "caps", caps, NULL);
  gst_caps_unref (caps);

  rtcpsrc = gst_element_factory_make ("udpsrc", "rtcpsrc");
  g_assert (rtcpsrc);
  g_object_set (rtcpsrc, "port", 5003, NULL);

  rtcpsink = gst_element_factory_make ("udpsink", "rtcpsink");
  g_assert (rtcpsink);
  
  //cojo el fd del socket del rtcpsrc y se lo fijo al rtcpsink y no hago la linea  de abajo
  //g_object_set (rtcpsink, "port", 5003, "host", DEST_HOST, NULL);
  GSocket* socket = NULL;
    g_object_get(rtcpsrc,  "socket", &socket, NULL);
    g_object_set(rtcpsink, "socket", socket, NULL);

    
  
  
  /* no need for synchronisation or preroll on the RTCP sink */
  g_object_set (rtcpsink, "async", FALSE, "sync", FALSE, NULL);

  gst_bin_add_many (GST_BIN (pipeline), rtpsrc, rtcpsrc, rtcpsink, NULL);

  /* the depayloading and decoding */
  audiodepay = gst_element_factory_make ("rtpamrdepay", "rtpamrdepay");
    g_assert (audiodepay);
  amrdecoder = gst_element_factory_make ("amrnbdec", "amrnbdec");
    g_assert (amrdecoder);
  wavenc = gst_element_factory_make   ("wavenc", "wavenc");
    g_assert (wavenc);
  audiores = gst_element_factory_make   ("audioresample", "audiores");
    g_assert (audiores);
  audioconvert = gst_element_factory_make  ("audioconvert", "audioconvert");
    g_assert (audioconvert);
  audiosink = gst_element_factory_make  ("filesink", "filesink");
    g_assert (audiosink);
    
    
  g_object_set(audiosink, "location", "recv_A.wav", NULL);

  /* add depayloading and playback to the pipeline and link */
  gst_bin_add_many (GST_BIN (pipeline), audiodepay, amrdecoder, audioconvert,  wavenc, audiosink, NULL);
 
  res = gst_element_link_many (audiodepay, amrdecoder, audioconvert,  wavenc,  audiosink, NULL);
  g_assert (res == TRUE);

  /* the rtpbin element */
  rtpbin = gst_element_factory_make ("rtpbin", "rtpbin");
  g_assert (rtpbin);

  gst_bin_add (GST_BIN (pipeline), rtpbin);

  /* now link all to the rtpbin, start by getting an RTP sinkpad for session 0 */
  srcpad = gst_element_get_static_pad (rtpsrc, "src");
  sinkpad = gst_element_get_request_pad (rtpbin, "recv_rtp_sink_0");
  lres = gst_pad_link (srcpad, sinkpad);
  g_assert (lres == GST_PAD_LINK_OK);
  gst_object_unref (srcpad);

  /* get an RTCP sinkpad in session 0 */
  srcpad = gst_element_get_static_pad (rtcpsrc, "src");
  sinkpad = gst_element_get_request_pad (rtpbin, "recv_rtcp_sink_0");
  lres = gst_pad_link (srcpad, sinkpad);
  g_assert (lres == GST_PAD_LINK_OK);
  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);

  /* get an RTCP srcpad for sending RTCP back to the sender */
  srcpad = gst_element_get_request_pad (rtpbin, "send_rtcp_src_0");
  sinkpad = gst_element_get_static_pad (rtcpsink, "sink");
  lres = gst_pad_link (srcpad, sinkpad);
  g_assert (lres == GST_PAD_LINK_OK);
  gst_object_unref (sinkpad);

  /* the RTP pad that we have to connect to the depayloader will be created
   * dynamically so we connect to the pad-added signal, pass the depayloader as
   * user_data so that we can link to it. */
  g_signal_connect (rtpbin, "pad-added", G_CALLBACK (pad_added_cb), audiodepay);
    /* give some stats when we receive RTCP */
  g_signal_connect (rtpbin, "on-ssrc-active", G_CALLBACK (on_ssrc_active_cb),
      audiodepay);
    
    
    g_signal_connect(rtpbin, "on-sender-timeout", G_CALLBACK(timeout_callback), (gpointer) GST_ON_SENDER_TIMEOUT);
    g_signal_connect(rtpbin, "on-timeout", G_CALLBACK(timeout_callback), (gpointer) GST_ON_TIMEOUT);
    g_signal_connect(rtpbin, "on-bye-ssrc", G_CALLBACK(timeout_callback), (gpointer) GST_ON_BYE_SSRC);
    g_signal_connect(rtpbin, "on-bye-timeout", G_CALLBACK(timeout_callback), (gpointer) GST_ON_BYE_TIME_OUT);

    
    


  /* set the pipeline to playing */
  g_print ("starting receiver pipeline\n");
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* we need to run a GLib main loop to get the messages */
  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_print ("stopping receiver pipeline\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_object_unref (pipeline);

  return 0;
}
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <gst/gst.h>
#include <gio/gio.h>


#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <string.h>
#include <math.h>


#define RTP_TX  5002
#define RTCP_TX 5003
#define RECV_FILENAME "tx.wav"


#define DEST_HOST "127.0.0.1"


static void print_source_stats (GObject * source)
{
  GstStructure *stats;
  gchar *str;

  /* get the source stats */
  g_object_get (source, "stats", &stats, NULL);

  /* simply dump the stats structure */
  str = gst_structure_to_string (stats);
  g_print ("source stats: %s\n", str);

  gst_structure_free (stats);
  g_free (str);
}

/* this function is called every second and dumps the RTP manager stats */
static gboolean print_stats (GstElement * rtpbin)
{
  GObject *session;
  GValueArray *arr;
  GValue *val;
  guint i;

  g_print ("***********************************\n");

  /* get session 0 */
  g_signal_emit_by_name (rtpbin, "get-internal-session", 0, &session);

  /* print all the sources in the session, this includes the internal source */
  g_object_get (session, "sources", &arr, NULL);

  for (i = 0; i < arr->n_values; i++) {
    GObject *source;

    val = g_value_array_get_nth (arr, i);
    source = g_value_get_object (val);

    print_source_stats (source);
  }
  g_value_array_free (arr);

  g_object_unref (session);

  return TRUE;
}



int main (int argc, char *argv[])
{
  GstElement *audiosrc_tx, *audioconv_tx, *audiores_tx, *audioenc_tx, *audiopay_tx, *wavparse_tx;
  GstElement *rtpbin, *rtpsink_tx, *rtcpsink_tx, *rtcpsrc_tx;
  GstElement *pipeline;
  GMainLoop *loop;
  GstPad *srcpad, *sinkpad;

  /* always init first */
  gst_init (&argc, &argv);

  /* the pipeline to hold everything */
  pipeline = gst_pipeline_new (NULL);
  g_assert (pipeline);

    /* the audio capture and format conversion */
  audiosrc_tx = gst_element_factory_make ("filesrc", "filesrc");
  g_object_set(audiosrc_tx, "location", "audioA.wav", NULL);
  
  wavparse_tx = gst_element_factory_make ("wavparse", "wavparse_tx");
  
  audioconv_tx = gst_element_factory_make ("audioconvert", "audioconv_tx");
  audiores_tx = gst_element_factory_make ("audioresample", "audiores_tx");
  
  /* the encoding and payloading */
  audioenc_tx = gst_element_factory_make ("amrnbenc", "amrnbenc");
  audiopay_tx = gst_element_factory_make ("rtpamrpay", "rtpamrpay");

  /* add capture and payloading to the pipeline and link */
  gst_bin_add_many (GST_BIN (pipeline), audiosrc_tx, wavparse_tx, audioconv_tx, audiores_tx,  audioenc_tx, audiopay_tx, NULL);

  if (!gst_element_link_many (audiosrc_tx, wavparse_tx, audioconv_tx, audiores_tx, audioenc_tx, audiopay_tx, NULL)) {
    g_error ("Failed to link audiosrc_tx, audioconv_tx, audiores_txample, "
        "audio encoder and audio payloader");
  }

  /* the rtpbin element */
  rtpbin = gst_element_factory_make ("rtpbin", "rtpbin");
  g_assert (rtpbin);

  gst_bin_add (GST_BIN (pipeline), rtpbin);

  /* the udp sinks and source we will use for RTP and RTCP */
  rtpsink_tx = gst_element_factory_make ("udpsink", "rtpsink_tx");
  g_assert (rtpsink_tx);
  g_object_set (rtpsink_tx, "port", RTP_TX, "host", DEST_HOST, NULL);

  rtcpsink_tx = gst_element_factory_make ("udpsink", "rtcpsink_tx");
  g_assert (rtcpsink_tx);
  g_object_set (rtcpsink_tx, "port", RTCP_TX, "host", DEST_HOST, NULL);
  g_object_set (rtcpsink_tx, "async", FALSE, "sync", FALSE, NULL);

  rtcpsrc_tx = gst_element_factory_make ("udpsrc", "rtcpsrc_tx");
  g_assert (rtcpsrc_tx);
  g_object_set (rtcpsrc_tx, "port", RTCP_TX, NULL);

  gst_bin_add_many (GST_BIN (pipeline), rtpsink_tx, rtcpsink_tx, rtcpsrc_tx, NULL);

  /* now link all to the rtpbin, start by getting an RTP sinkpad for session 0 */
  sinkpad = gst_element_get_request_pad (rtpbin, "send_rtp_sink_0");
  srcpad = gst_element_get_static_pad (audiopay_tx, "src");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK)
    g_error ("Failed to link audio payloader to rtpbin");
  gst_object_unref (srcpad);

  /* get the RTP srcpad that was created when we requested the sinkpad above and
   * link it to the rtpsink_tx sinkpad*/
  srcpad = gst_element_get_static_pad (rtpbin, "send_rtp_src_0");
  sinkpad = gst_element_get_static_pad (rtpsink_tx, "sink");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK)
    g_error ("Failed to link rtpbin to rtpsink_tx");
  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);

  /* get an RTCP srcpad for sending RTCP to the receiver */
  srcpad = gst_element_get_request_pad (rtpbin, "send_rtcp_src_0");
  sinkpad = gst_element_get_static_pad (rtcpsink_tx, "sink");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK)
    g_error ("Failed to link rtpbin to rtcpsink_tx");
  gst_object_unref (sinkpad);

  /* we also want to receive RTCP, request an RTCP sinkpad for session 0 and
   * link it to the srcpad of the udpsrc for RTCP */
  srcpad = gst_element_get_static_pad (rtcpsrc_tx, "src");
  sinkpad = gst_element_get_request_pad (rtpbin, "recv_rtcp_sink_0");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK)
    g_error ("Failed to link rtcpsrc_tx to rtpbin");
  gst_object_unref (srcpad);

  /* set the pipeline to playing */
  g_print ("starting sender pipeline\n");
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* print stats every second */
  g_timeout_add_seconds (1, (GSourceFunc) print_stats, rtpbin);

  /* we need to run a GLib main loop to get the messages */
  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_print ("stopping sender pipeline\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);

  return 0;
}

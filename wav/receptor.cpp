#include <string.h>
#include <math.h>
#include <stdio.h>
#include <gst/gst.h>
#include <gio/gio.h>


#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <string.h>
#include <math.h>


#define RTP_TX  5000
#define RTCP_TX 5001
#define RTP_RX  5002
#define RTCP_RX 5003

#define RECV_FILENAME "recibidoB.wav"

#define AUDIO_CAPS "application/x-rtp,media=(string)audio,clock-rate=(int)8000,encoding-name=(string)AMR,encoding-params=(string)1,octet-align=(string)1" 


#define GST_ON_SENDER_TIMEOUT   "on-sender-timeout"
#define GST_ON_TIMEOUT          "on-timeout"
#define GST_ON_BYE_SSRC         "on-bye-ssrc"
#define GST_ON_BYE_TIME_OUT     "on-bye-timeout"


#define DEST_HOST "127.0.0.1"


  GMainLoop *loop_tx;
  GMainLoop *loop;

  
  static void print_source_stats (GObject * source)
{
  GstStructure *stats=NULL;
  gchar *str;

  /* get the source stats */
  g_object_get (source, "stats", &stats, NULL);

  if(stats==NULL)
    return;
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

  
static void on_ssrc_active_cb (GstElement * rtpbin, guint sessid, guint ssrc,  GstElement * depay)
{
  GObject *session, *isrc=NULL, *osrc=NULL;

  g_print ("got RTCP from session %u, SSRC %u\n", sessid, ssrc);

  /* get the right session */
  g_signal_emit_by_name (rtpbin, "get-internal-session", sessid, &session);

  /* get the internal source (the SSRC allocated to us, the receiver */
  g_object_get (session, "internal-source", &isrc, NULL);
  if(isrc)
    print_source_stats (isrc);

  /* get the remote source that sent us RTCP */
  g_signal_emit_by_name (session, "get-source-by-ssrc", ssrc, &osrc);
  if(osrc)
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

static gboolean my_bus_callback (GstBus     *bus, GstMessage *message, gpointer    data)
{
  g_print ("Got %s message\n", GST_MESSAGE_TYPE_NAME (message));

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR: {
      GError *err;
      gchar *debug;

      gst_message_parse_error (message, &err, &debug);
      g_print ("Error: %s\n", err->message);
      g_error_free (err);
      g_free (debug);

      g_main_loop_quit (loop_tx);
      break;
    }
    case GST_MESSAGE_EOS:
      /* end-of-stream */
      g_main_loop_quit (loop_tx);
      break;
    default:
      /* unhandled message */
      break;
  }
  
}


GstElement *pipeline;
static void timeout_callback(GstElement* element, guint session, guint ssrc, gpointer user_data)
{
    if (user_data == NULL)
    	printf( "GST: Sending EOS TO THE pipeline !!!!\n");
    else 
        printf( "GST: Sending EOS TO THE pipeline in case:%s !!!!\n",(char *)user_data);
	
    
    gst_element_send_event(pipeline, gst_event_new_eos());
    g_main_loop_quit (loop);
}

void *envia( void *ptr)
{
    GstElement *audiosrc_tx, *audioconv_tx, *audiores_tx, *audioenc_tx, *audiopay_tx, *wavparse_tx;
  GstElement *rtpbin, *rtpsink_tx, *rtcpsink_tx, *rtcpsrc_tx;
  GstElement *pipeline;
  
  GstPad *srcpad, *sinkpad;

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

  
    GstBus* bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  guint bus_watch_id = gst_bus_add_watch (bus, my_bus_callback, NULL);
  gst_object_unref (bus);

  
  /* set the pipeline to playing */
  g_print ("starting sender pipeline\n");
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* print stats every second */
  g_timeout_add_seconds (1, (GSourceFunc) print_stats, rtpbin);

  /* we need to run a GLib main loop_tx to get the messages */
  loop_tx = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop_tx);

  g_print ("stopping sender pipeline\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);
  
}

void *recibe( void *ptr)
{
   GstElement *rtpbin, *rtpsrc_rx, *rtcpsrc_rx, *rtcpsink_rx;
  GstElement *audiodepay_rx, *wavenc_rx, *audioconvert_rx, *audiosink_rx, *amrdecoder_rx;

  
  GstCaps *caps_rx;
  gboolean res;
  GstPadLinkReturn lres;
  GstPad *srcpad, *sinkpad;


  /* the pipeline to hold everything */
  pipeline = gst_pipeline_new (NULL);
  g_assert (pipeline);

  /* the udp src and source we will use for RTP and RTCP */
  rtpsrc_rx = gst_element_factory_make ("udpsrc", "rtpsrc_rx");
  g_assert (rtpsrc_rx);
  g_object_set (rtpsrc_rx, "port", RTP_RX, NULL);
  /* we need to set caps_rx on the udpsrc for the RTP data */
  caps_rx = gst_caps_from_string (AUDIO_CAPS);
  g_object_set (rtpsrc_rx, "caps", caps_rx, NULL);
  gst_caps_unref (caps_rx);

  rtcpsrc_rx = gst_element_factory_make ("udpsrc", "rtcpsrc_rx");
  g_assert (rtcpsrc_rx);
  g_object_set (rtcpsrc_rx, "port", RTCP_RX, NULL);

  rtcpsink_rx = gst_element_factory_make ("udpsink", "rtcpsink_rx");
  g_assert (rtcpsink_rx);
  
  GSocket* socket = NULL;
  g_object_get(rtcpsrc_rx,  "socket", &socket, NULL);
  g_object_set(rtcpsink_rx, "socket", socket, NULL);

    
  /* no need for synchronisation or preroll on the RTCP sink */
  g_object_set (rtcpsink_rx, "async", FALSE, "sync", FALSE, NULL);

  gst_bin_add_many (GST_BIN (pipeline), rtpsrc_rx, rtcpsrc_rx, rtcpsink_rx, NULL);

  /* the depayloading and decoding */
  audiodepay_rx = gst_element_factory_make ("rtpamrdepay", "rtpamrdepay_tx");
  amrdecoder_rx = gst_element_factory_make ("amrnbdec", "amrnbdec");
  wavenc_rx = gst_element_factory_make   ("wavenc", "wavenc_rx");
  audioconvert_rx = gst_element_factory_make  ("audioconvert", "audioconvert_rx");
  audiosink_rx = gst_element_factory_make  ("filesink", "filesink");
    
    
  g_object_set(audiosink_rx, "location", RECV_FILENAME, NULL);

  /* add depayloading and playback to the pipeline and link */
  gst_bin_add_many (GST_BIN (pipeline), audiodepay_rx, amrdecoder_rx, audioconvert_rx,  wavenc_rx, audiosink_rx, NULL);
 
  res = gst_element_link_many (audiodepay_rx, amrdecoder_rx, audioconvert_rx,  wavenc_rx,  audiosink_rx, NULL);
  g_assert (res == TRUE);

  /* the rtpbin element */
  rtpbin = gst_element_factory_make ("rtpbin", "rtpbin");
  g_assert (rtpbin);

  gst_bin_add (GST_BIN (pipeline), rtpbin);

  /* now link all to the rtpbin, start by getting an RTP sinkpad for session 0 */
  srcpad = gst_element_get_static_pad (rtpsrc_rx, "src");
  sinkpad = gst_element_get_request_pad (rtpbin, "recv_rtp_sink_1");
  lres = gst_pad_link (srcpad, sinkpad);
  g_assert (lres == GST_PAD_LINK_OK);
  gst_object_unref (srcpad);

  /* get an RTCP sinkpad in session 0 */
  srcpad = gst_element_get_static_pad (rtcpsrc_rx, "src");
  sinkpad = gst_element_get_request_pad (rtpbin, "recv_rtcp_sink_1");
  lres = gst_pad_link (srcpad, sinkpad);
  g_assert (lres == GST_PAD_LINK_OK);
  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);

  /* get an RTCP srcpad for sending RTCP back to the sender */
  srcpad = gst_element_get_request_pad (rtpbin, "send_rtcp_src_1");
  sinkpad = gst_element_get_static_pad (rtcpsink_rx, "sink");
  lres = gst_pad_link (srcpad, sinkpad);
  g_assert (lres == GST_PAD_LINK_OK);
  gst_object_unref (sinkpad);

  /* the RTP pad that we have to connect to the depayloader will be created
   * dynamically so we connect to the pad-added signal, pass the depayloader as
   * user_data so that we can link to it. */
  g_signal_connect (rtpbin, "pad-added", G_CALLBACK (pad_added_cb), audiodepay_rx);
    /* give some stats when we receive RTCP */
  g_signal_connect (rtpbin, "on-ssrc-active", G_CALLBACK (on_ssrc_active_cb),     audiodepay_rx);
    
  g_signal_connect(rtpbin, "on-sender-timeout", G_CALLBACK(timeout_callback), (gpointer) GST_ON_SENDER_TIMEOUT);
  g_signal_connect(rtpbin, "on-timeout",        G_CALLBACK(timeout_callback), (gpointer) GST_ON_TIMEOUT);
  g_signal_connect(rtpbin, "on-bye-ssrc",       G_CALLBACK(timeout_callback), (gpointer) GST_ON_BYE_SSRC);
  g_signal_connect(rtpbin, "on-bye-timeout",    G_CALLBACK(timeout_callback), (gpointer) GST_ON_BYE_TIME_OUT);

  GstBus* bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  guint bus_watch_id = gst_bus_add_watch (bus, my_bus_callback, NULL);
  gst_object_unref (bus);


  /* set the pipeline to playing */
  g_print ("starting receiver pipeline\n");
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  
  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_print ("stopping receiver pipeline\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_object_unref (pipeline);

  return ;
  
}
int main (int argc, char *argv[])
{

  /* always init first */
  gst_init (&argc, &argv);


     pthread_t thread1, thread2;

     const char *message1 = "Thread 1";
     const char *message2 = "Thread 2";
     int  iret1, iret2;

     iret1 = pthread_create( &thread1, NULL, recibe, (void*) message1);
     usleep(100000);
     iret2 = pthread_create( &thread2, NULL, envia, (void*) message2);

     pthread_join( thread1, NULL);
     pthread_join( thread2, NULL);


  return 0;
}

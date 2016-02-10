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
#define RTP_RX  5000
#define RTCP_RX 5001

#define RECV_FILENAME "recA.mp4"


#define VIDEO_CAPS "application/x-rtp,media=(string)video,clock-rate=(int)90000,encoding-name=(string)H264" 

#define GST_ON_SENDER_TIMEOUT   "on-sender-timeout"
#define GST_ON_TIMEOUT          "on-timeout"
#define GST_ON_BYE_SSRC         "on-bye-ssrc"
#define GST_ON_BYE_TIME_OUT     "on-bye-timeout"


#define DEST_HOST "127.0.0.1"


  GMainLoop *loop_tx;
  GMainLoop *loop;
static gboolean bus_call (GstBus *bus, GstMessage *msg, gpointer data) 
	{ 
	  GMainLoop *loop = (GMainLoop *) data; 
	  switch (GST_MESSAGE_TYPE (msg)) { 
	      case GST_MESSAGE_EOS: 
	      g_print ("End of stream\n"); 
	      g_main_loop_quit (loop); 
	      break; 
	    case GST_MESSAGE_ERROR: { 
	gchar *debug; 
	GError *error; 
	gst_message_parse_error (msg, &error, &debug); 
	g_free (debug); 
	g_printerr ("Error: %s\n", error->message); 
	g_error_free (error); 
	g_main_loop_quit (loop); 
	break; 
	} 
	default: 
	break; 
	} 
	return TRUE; 
	} 
	static void 
	on_pad_added (GstElement *element, 
	GstPad *pad, 
	gpointer data) 
	{ 
	GstPad *sinkpad; 
	GstElement *decoder = (GstElement *) data; 
	/* We can now link this pad with the vorbis-decoder sink pad */ 
	g_print ("Dynamic pad created, linking demuxer/decoder\n"); 
	sinkpad = gst_element_get_static_pad (decoder, "sink"); 
	gst_pad_link (pad, sinkpad); 
	gst_object_unref (sinkpad); 
	} 
 
  
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
  //g_print ("Got %s message\n", GST_MESSAGE_TYPE_NAME (message));

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
 GMainLoop *loop; 
	GstElement *pipeline, *source, *demuxer, *decoder, *conv, *sink; 
	GstBus *bus; 
	/* Initialisation */ 
	gst_init (&argc, &argv); 
	loop = g_main_loop_new (NULL, FALSE); 
	/* Check input arguments */ 
	if (argc != 2) { 
	g_printerr ("Usage: %s <Ogg/Vorbis filename>\n", argv[0]); 
	return -1; 
	} 
	/* Create gstreamer elements */ 
	pipeline = gst_pipeline_new ("audio-player"); 
	source = gst_element_factory_make ("filesrc", "file-source"); 
	demuxer = gst_element_factory_make ("qtdemux", "demuxer"); 
	decoder = gst_element_factory_make ("avdec_h264", "decoder"); 
	sink = gst_element_factory_make ("udpsink", "video-output"); 
	
	  g_object_set (sink, "port", RTP_TX, "host", DEST_HOST, NULL);
  
	
	if (!pipeline || !source || !demuxer || !decoder || !sink) { 
	g_printerr ("One element could not be created. Exiting.\n"); 
	return -1; 
	} 
	/* Set up the pipeline */ 
	/* we set the input filename to the source element */ 
	g_object_set (G_OBJECT (source), "location", argv[1], NULL); 
	/* we add a message handler */ 
	bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline)); 
	gst_bus_add_watch (bus, bus_call, loop); 
	gst_object_unref (bus); 
	/* we add all elements into the pipeline */ 
	gst_bin_add_many (GST_BIN (pipeline), 
	source, demuxer, decoder, sink, NULL); 
	/* we link the elements together */ 
	///gst_element_link (source, demuxer); 
	//gst_element_link_many (decoder, sink, NULL); 
	gst_element_link_many (source, demuxer, sink, NULL); 
	g_signal_connect (demuxer, "pad-added", G_CALLBACK (on_pad_added), sink); 
	/* note that the demuxer will be linked to the decoder dynamically. 
	The reason is that Ogg may contain various streams (for example 
	audio and video). The source pad(s) will be created at run time, 
	by the demuxer when it detects the amount and nature of streams. 
	Therefore we connect a callback function which will be executed 
	when the "pad-added" is emitted.*/ 
	/* Set the pipeline to "playing" state*/ 
	g_print ("Now playing: %s\n", argv[1]); 
	gst_element_set_state (pipeline, GST_STATE_PLAYING); 
	/* Iterate */ 
	g_print ("Running...\n"); 
	g_main_loop_run (loop); 
	/* Out of the main loop, clean up nicely */ 
	g_print ("Returned, stopping playback\n"); 
	gst_element_set_state (pipeline, GST_STATE_NULL); 
	g_print ("Deleting pipeline\n"); 
	gst_object_unref (GST_OBJECT (pipeline)); 
	
}

void *recibe( void *ptr)
{
   GstElement *rtpbin, *rtpsrc_rx;
  GstElement *rtph264depay, *mp4mux, *filesink, *h264parse;

  
  GstCaps *caps_rx;
  gboolean res;
  GstPadLinkReturn lres;
  GstPad *srcpad, *sinkpad;


  pipeline = gst_pipeline_new (NULL);
  g_assert (pipeline);

  
  h264parse = gst_element_factory_make ("h264parse", NULL);
  
  rtpsrc_rx = gst_element_factory_make ("udpsrc", "rtpsrc_rx");
  g_assert (rtpsrc_rx);
  g_object_set (rtpsrc_rx, "port", RTP_RX, NULL);
  
  caps_rx = gst_caps_from_string (VIDEO_CAPS);
  g_object_set (rtpsrc_rx, "caps", caps_rx, NULL);
  gst_caps_unref (caps_rx);

  
  
  rtph264depay = gst_element_factory_make ("rtph264depay", "rtph264depay");
  mp4mux = gst_element_factory_make  ("mp4mux", "mp4mux");
  filesink = gst_element_factory_make  ("filesink", "filesink");
    
    
  g_object_set(filesink, "location", RECV_FILENAME, NULL);

  
  gst_bin_add_many (GST_BIN (pipeline), rtpsrc_rx, rtph264depay, h264parse, mp4mux,  filesink, NULL);
  
  res = gst_element_link_many (rtpsrc_rx, rtph264depay, h264parse, mp4mux, filesink, NULL);
  g_assert (res == TRUE);


  
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

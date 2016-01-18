#include <stdio.h>
#include <gst/gst.h>
#include <gio/gio.h>


#define GST_ON_SENDER_TIMEOUT   "on-sender-timeout"
#define GST_ON_TIMEOUT          "on-timeout"
#define GST_ON_BYE_SSRC         "on-bye-ssrc"
#define GST_ON_BYE_TIME_OUT     "on-bye-timeout"

#define GLIB_DISABLE_DEPRECATION_WARNINGS

#define RTCP_REMOTE_PORT 5001
#define RTP_REMOTE_PORT  5000
#define RTP_REMOTE_PORT  5000
#define REMOTE_IP        "127.0.0.1"
#define MIME_TYPE        "AMR"
#define PAYLOAD_ID       96
#define SDP_BANDWIDTH     1
#define CLOCK_RATE        8000

static void pad_removed_cb(GstElement * rtpbin, GstPad * new_pad,
gpointer user_data /*GstElement * depay */)
{
    printf("pad removed: %s\n", GST_PAD_NAME(new_pad));

}

GstPipeline   *pipeline;
static void timeout_callback(GstElement* element, guint session, guint ssrc, gpointer user_data)
{
    if (user_data == NULL)
    	printf("GST: Sending EOS TO THE pipeline !!!!\n");
    else 
        printf("GST: Sending EOS TO THE pipeline in case:%s !!!!\n",(char *)user_data);
    
    //OJO JULIAN HAS COMENTADO ESTO PORQUE NO TE COMPILABA EN TU PC
    //gst_element_send_event(pipeline, gst_event_new_eos());
}



/* print the stats of a source */
static void
print_source_stats(GObject * source)
{
    GstStructure *stats;
    gchar *str;

    g_return_if_fail(source != NULL);

    /* get the source stats */
    g_object_get(source, "stats", &stats, NULL);

    /* simply dump the stats structure */
    str = gst_structure_to_string(stats);
    g_print("source stats: %s\n", str);

    gst_structure_free(stats);
    g_free(str);
}

/* will be called when rtpbin signals on-ssrc-active. It means that an RTCP
* packet was received from another source. */
static void
on_ssrc_active_cb(GstElement * rtpbin, guint sessid, guint ssrc,
GstElement * depay)
{
    GObject *session, *isrc, *osrc;

    printf("GST: Sending EOS TO THE pipeline (on-ssrc-active-cb) !!!!\n");

    g_print("got RTCP from session %u, SSRC %u\n", sessid, ssrc);

    /* get the right session */
    g_signal_emit_by_name(rtpbin, "get-internal-session", sessid, &session);

    /* get the internal source (the SSRC allocated to us, the receiver */
    g_object_get(session, "internal-source", &isrc, NULL);
    print_source_stats(isrc);

    /* get the remote source that sent us RTCP */
    g_signal_emit_by_name(session, "get-source-by-ssrc", ssrc, &osrc);
    print_source_stats(osrc);
}


int main()
{
    gst_init(NULL, NULL);

    // Elements for generate Output Stream
    GstElement *filesrc, *wavparse, *audioresample, *audioconvert_in, *amrencoder, *rtpamrpay;
    //Elements for network operations
    GstElement *rtpbin, *hrtpsink, *hrtcpsink, *hrtcpsink_prueba, *receiver_rtcp_in, *h264recv;
    //Element for save input stream
    GstElement *rtpamrdepay, *amrdecoder, *audioconvert_out, *wavenc, *filesink;

    //Audio stream out (sender) plugins
    filesrc         = gst_element_factory_make("filesrc",       NULL);
    wavparse        = gst_element_factory_make("wavparse",      NULL);
    audioresample   = gst_element_factory_make("audioresample", NULL);
    audioconvert_in = gst_element_factory_make("audioconvert", NULL);
    rtpamrpay       = gst_element_factory_make("rtpamrpay",     NULL);

    amrencoder = gst_element_factory_make("amrnbenc", NULL);
    amrdecoder = gst_element_factory_make("amrnbdec", NULL);

    g_object_set(amrencoder, "band-mode", SDP_BANDWIDTH, NULL);

    //Socket plugins
    rtpbin           = gst_element_factory_make("rtpbin",  "rtpbin");
    hrtpsink         = gst_element_factory_make("udpsink", "hrtpsink");
    hrtcpsink        = gst_element_factory_make("udpsink", "hrtcpsink");

    hrtcpsink_prueba = gst_element_factory_make("udpsink", "hrtcpsink_prueba");

    receiver_rtcp_in = gst_element_factory_make("udpsrc",  "receiver_rtcp_in");
    h264recv         = gst_element_factory_make("udpsrc",  "h264recv");


    //Audio stream in (receiver) plugins
    rtpamrdepay      = gst_element_factory_make("rtpamrdepay",  NULL);
    audioconvert_out = gst_element_factory_make("audioconvert", NULL);
    wavenc           = gst_element_factory_make("wavenc",       NULL);
    filesink         = gst_element_factory_make("filesink",     NULL);

    g_object_set(filesrc,  "location", "audioA.wav",          NULL);
    g_object_set(filesink, "location", "recorded_audioA.wav", NULL);

    //GST: Set payload size as negotiated
    g_object_set(rtpamrpay, "pt", 96, NULL);


    GstCaps     *tmp_caps;
    tmp_caps = gst_caps_new_simple("application/x-rtp", NULL);
    printf("GST: receiver_rtcp_in port %d address %s\n", RTCP_REMOTE_PORT, REMOTE_IP);
    g_object_set(receiver_rtcp_in, "port", RTCP_REMOTE_PORT, "address", REMOTE_IP, "caps", tmp_caps, NULL);

    GstCaps     *audio_caps;
    audio_caps = gst_caps_new_simple("application/x-rtp",
        "media", G_TYPE_STRING, "audio",
        "payload", G_TYPE_INT,    PAYLOAD_ID,
        "clock-rate", G_TYPE_INT, CLOCK_RATE,
        "encoding-name", G_TYPE_STRING, MIME_TYPE,
        "encoding-params", G_TYPE_STRING, "1",
        NULL);

    printf("GST: rtp port %d address %s\n", RTP_REMOTE_PORT, REMOTE_IP);
    g_object_set(h264recv, "port", RTP_REMOTE_PORT, "address", REMOTE_IP, "caps", audio_caps, NULL);

    //GSocket * s = g_socket_new_from_fd(chatp->socket.sockfd, NULL);
    //GSocket * s2 = g_socket_new_from_fd(chatp->rtcp.sockfd, NULL);

    //printf("Fijamos socket de hrtpsink\n");
    //fflush(stdout);
    //g_object_set(hrtpsink, "socket", s, NULL);
    //printf("Fijamos socket de h264recv\n");
    //fflush(stdout);
    //g_object_set(h264recv, "socket", s, NULL);

    printf("Fijamos async de hrtpsink\n");
    fflush(stdout);
    g_object_set(hrtpsink, "async", FALSE, NULL);
    printf("Fijamos port de hrtpsink:%d \n", RTP_REMOTE_PORT);
    fflush(stdout);
    g_object_set(hrtpsink, "port", RTP_REMOTE_PORT, NULL);
    printf("Fijamos host de hrtpsink:%s \n", REMOTE_IP);
    fflush(stdout);
    g_object_set(hrtpsink, "host", REMOTE_IP, NULL);
    printf("Fijamos ts-offset de hrtpsink\n");
    fflush(stdout);
    g_object_set(hrtpsink, "ts-offset", 0, NULL);
    
    //printf("Fijamos socket de hrtcpsink\n");
    //fflush(stdout);
//    g_object_set(hrtcpsink, "socket", s2, NULL);
    printf("Fijamos port de hrtcpsink:%d \n", RTCP_REMOTE_PORT);
    fflush(stdout);
    g_object_set(hrtcpsink, "port", RTCP_REMOTE_PORT, NULL);
    printf("Fijamos host de hrtcpsink:%s \n", REMOTE_IP);
    fflush(stdout);
    g_object_set(hrtcpsink, "host", REMOTE_IP, NULL);
    printf("Fijamos sync de hrtcpsink\n");
    fflush(stdout);
 //   printf("GST: hrtcpsink -> sync parameter set\n");
    g_object_set(hrtcpsink, "sync", FALSE, NULL);
    printf("Fijamos async de hrtcpsink\n");
    fflush(stdout);
 //   printf("GST: hrtcpsink -> async parameter set\n");
    g_object_set(hrtcpsink, "async", FALSE, NULL);


    printf("Fijamos socket de hrtcpsink_prueba\n");
    fflush(stdout);
    //    printf("GST: hrtcpsink -> socket parameter set\n"); */
//    g_object_set(hrtcpsink_prueba, "socket", s2, NULL);
    printf("Fijamos port de hrtcpsink:%d \n", RTCP_REMOTE_PORT);
    fflush(stdout);
    //   printf("GST: hrtcpsink -> port parameter set to %d \n", RTCP_REMOTE_PORT);
    g_object_set(hrtcpsink_prueba, "port", RTCP_REMOTE_PORT, NULL);
    printf("Fijamos host de hrtcpsink:%s \n", REMOTE_IP);
    fflush(stdout);
    //   printf("GST: hrtcpsink -> host parameter set to %s \n", REMOTE_IP);
    g_object_set(hrtcpsink_prueba, "host", REMOTE_IP, NULL);
    printf("Fijamos sync de hrtcpsink\n");
    fflush(stdout);
    //   printf("GST: hrtcpsink -> sync parameter set\n");
    g_object_set(hrtcpsink_prueba, "sync", FALSE, NULL);
    printf("Fijamos async de hrtcpsink\n");
    fflush(stdout);
    //   printf("GST: hrtcpsink -> async parameter set\n");
    g_object_set(hrtcpsink_prueba, "async", FALSE, NULL);

 //   printf("GST: receiver_rtcp_in -> socket parameter set\n");
    //g_object_set(receiver_rtcp_in, "socket", s2, NULL);
    
    
  


    pipeline = GST_PIPELINE(gst_pipeline_new(NULL));
    gst_bin_add_many(GST_BIN(pipeline), rtpbin, filesrc, wavparse, audioresample, amrencoder, rtpamrpay, hrtpsink, hrtcpsink, NULL);

    if (gst_element_link_many(filesrc, wavparse, audioresample, amrencoder, rtpamrpay, NULL) != TRUE)
        printf( "GST: Problem linking sender part!!!!!!!\n");

    gst_element_link_pads_filtered(rtpamrpay, "src", rtpbin, "send_rtp_sink_0", NULL);
    gst_element_link_pads_filtered(rtpbin, "send_rtp_src_0", hrtpsink, "sink", NULL);
    gst_element_link_pads_filtered(rtpbin, "send_rtcp_src_0", hrtcpsink, "sink", NULL);

    gst_bin_add_many(GST_BIN(pipeline), receiver_rtcp_in, h264recv, rtpamrdepay, amrdecoder, audioconvert_out, wavenc, filesink, hrtcpsink_prueba, NULL);
    if (gst_element_link_many(h264recv, rtpamrdepay, amrdecoder, audioconvert_out, wavenc, filesink, NULL) != TRUE)
    {
        printf( "GST: problem linking elements\n");
    }

    gst_element_link_pads_filtered(rtpbin, "recv_rtp_src_0", rtpamrdepay, "sink", NULL);
 //   gst_element_link_pads_filtered(rtpamrdepay, "sink", rtpbin, "recv_rtp_src_0", NULL);
    gst_element_link_pads_filtered(h264recv, "src", rtpbin, "recv_rtp_sink_0", NULL);
    gst_element_link_pads_filtered(receiver_rtcp_in, "src", rtpbin, "recv_rtcp_sink_0", NULL);
    gst_element_link_pads_filtered(rtpbin, "send_rtcp_sink_0", hrtcpsink_prueba, "sink", NULL);

    /* the RTP pad that we have to connect to the depayloader will be created
    * dynamically so we connect to the pad-added signal, pass the depayloader as
    * user_data so that we can link to it. */
    g_signal_connect(rtpbin, "pad-removed", G_CALLBACK(pad_removed_cb), NULL);
    g_signal_connect(rtpbin, "on-sender-timeout", G_CALLBACK(timeout_callback), (gpointer) GST_ON_SENDER_TIMEOUT);
    g_signal_connect(rtpbin, "on-timeout", G_CALLBACK(timeout_callback), (gpointer) GST_ON_TIMEOUT);
    g_signal_connect(rtpbin, "on-bye-ssrc", G_CALLBACK(timeout_callback), (gpointer) GST_ON_BYE_SSRC);
    g_signal_connect(rtpbin, "on-bye-timeout", G_CALLBACK(timeout_callback), (gpointer) GST_ON_BYE_TIME_OUT);

    /* give some stats when we receive RTCP */
    g_signal_connect(rtpbin, "on-ssrc-active", G_CALLBACK(on_ssrc_active_cb), NULL);


    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);


    /* we need to run a GLib main loop to get the messages */
//     GMainLoop *loop;
//     loop = g_main_loop_new(NULL, FALSE);
//     g_main_loop_run(loop);

    
    GstBus     *bus = gst_element_get_bus(pipeline);
    
    GstMessage *msg = gst_bus_timed_pop_filtered(bus,10 * (unsigned long long int)1100000, GST_MESSAGE_EOS);


    if (msg){
        printf("GST: Freeing message\n");
        gst_message_unref(msg);
    }
    

    //Now we are going to get the statistics
    GObject *session;
    GValueArray *arr;
    GValue *val;
    guint i;

    /* get session 0 */
    g_signal_emit_by_name(rtpbin, "get-internal-session", 0, &session);

    /* print all the sources in the session, this includes the internal source */
    g_object_get(session, "sources", &arr, NULL);

    printf("GST: Returning %d number of statistics\n", arr->n_values);

    for (i = 0; i < arr->n_values; i++) {
        GObject *source;

        val = arr->values + i;
	//ojo julian has comentado la siguiente linea para que compilara
//        source = g_value_get_object(val);

        GstStructure *stats;

        g_object_get(source, "stats", &stats, NULL);

        /* simply dump the stats structure */
        gchar *str;
        str = gst_structure_to_string(stats);
        g_print("source stats: %s\n", str);
        g_free(str);

    }
   
    

    return 0;
}

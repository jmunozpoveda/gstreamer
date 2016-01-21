#include <string.h>
#include <math.h>
#include <stdio.h>
#include <gst/gst.h>
#include <gio/gio.h>


#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <string.h>
#include <math.h>




#define RTP_REMOTE_PORT  5000
#define RTCP_REMOTE_PORT 5001
#define RTP_LOCAL_PORT   5002
#define RTCP_LOCAL_PORT  5003


#define FILENAME "audioA.wav"
#define RECV_FILENAME "recibidoA.wav"




#define LOCAL_IP "172.26.0.111"
#define GST_ON_SENDER_TIMEOUT   "on-sender-timeout"
#define GST_ON_TIMEOUT          "on-timeout"
#define GST_ON_BYE_SSRC         "on-bye-ssrc"
#define GST_ON_BYE_TIME_OUT     "on-bye-timeout"



static void pad_removed_cb(GstElement * rtpbin, GstPad * new_pad,
gpointer user_data /*GstElement * depay */)
{
    printf("pad removed: %s\n", GST_PAD_NAME(new_pad));

}


static void print_source_stats(GObject * source)
{
    GstStructure *stats;
    gchar *str;

    if (source == NULL)
        return;

    /* get the source stats */
    g_object_get(source, "stats", &stats, NULL);

    /* simply dump the stats structure */
    str = gst_structure_to_string(stats);
    g_print("source stats: %s\n", str);
    printf("GST: source stats: %s\n", str);

    gst_structure_free(stats);
    g_free(str);
}



/* this function is called every second and dumps the RTP manager stats */
static gboolean print_stats_NEW(GstElement * rtpbin)
{
    GObject *session;
    GValueArray *arr;
    GValue *val;
    guint i;

    printf("**************** ESTADISTICAS DE EMISION *******************\n"); fflush(stdout);
    printf("**************** ESTADISTICAS DE EMISION *******************\n");

    /* get session 0 */
    g_signal_emit_by_name(rtpbin, "get-internal-session", 0, &session);

    /* print all the sources in the session, this includes the internal source */
    g_object_get(session, "sources", &arr, NULL);

    for (i = 0; i < arr->n_values; i++) {
        GObject *source;

        val = arr->values + i;
        source = g_value_get_object(val);

        print_source_stats(source);

        GstStructure *stats;
        g_object_get(source, "stats", &stats, NULL);

    }
    //    g_value_array_free(arr);

    g_object_unref(session);

    return TRUE;
}


/* will be called when rtpbin signals on-ssrc-active. It means that an RTCP
* packet was received from another source. */
static void on_ssrc_active_cb(GstElement * rtpbin, guint sessid, guint ssrc, GstElement * depay)
{
    GObject *session, *isrc, *osrc;


    printf("**************** ESTADISTICAS DE RECEPCION *******************\n");
    printf("**************** ESTADISTICAS DE RECEPCION *******************\n"); fflush(stdout);


    g_print("got RTCP from session %u, SSRC %u\n", sessid, ssrc);

    /* get the right session */
    g_signal_emit_by_name(rtpbin, "get-internal-session", sessid, &session);

    /* get the internal source (the SSRC allocated to us, the receiver */
    g_object_get(session, "internal-source", &isrc, NULL);
    printf("-------------- COMIENZO internal-sourcec -------------- - \n");
    print_source_stats(isrc);
    printf("-------------- FIN internal-sourcec -------------- - \n");

    /* get the remote source that sent us RTCP */
    g_signal_emit_by_name(session, "get-source-by-ssrc", ssrc, &osrc);
    printf("-------------- COMIENZO get-source-by-ssrc -------------- - \n");
    print_source_stats(osrc);
    printf("-------------- fin get-source-by-ssrc -------------- - \n");

    GValueArray *arr;
    GValue *val;
    guint i;

    g_object_get(session, "sources", &arr, NULL);
    
    printf("-------------- COMIENZO sources-------------- - \n");
    printf("GST: ON SSRC Returning %d number of statistics\n", arr->n_values);

    for (i = 0; i < arr->n_values; i++) {
        GObject *source;

        val = arr->values + i;
        source = g_value_get_object(val);

        GstStructure *stats;

        g_object_get(source, "stats", &stats, NULL);

        /* simply dump the stats structure */
        gchar *str;
        str = gst_structure_to_string(stats);
        g_print("source stats: %s\n", str);
        printf("source stats: %s\n", str);
        g_free(str);

    }
    printf("-------------- FIN sources-------------- - \n");
    
}

GstPipeline   *pipeline;
static void timeout_callback(GstElement* element, guint session, guint ssrc, gpointer user_data)
{
    if (user_data == NULL)
    	printf("GST: Sending EOS TO THE pipeline !!!!\n");
    else 
        printf("GST: Sending EOS TO THE pipeline in case:%s !!!!\n",(char *)user_data);fflush(stdout);
	gst_element_send_event(GST_ELEMENT(pipeline), gst_event_new_eos());
}


int main(int argc, char *argv[])
{
    gboolean usingAmrNb = FALSE;

    gst_init(&argc, &argv);

    // Elements for generate Output Stream
    GstElement *filesrc, *wavparse, *audioresample, *audioconvert_in, *amrencoder, *rtpamrpay;
    //Elements for network operations
    GstElement *rtpbin, *hrtpsink, *hrtcpsink, *receiver_rtcp_in, *h264recv;
    //Element for save input stream
    GstElement *rtpamrdepay, *amrdecoder, *audioconvert_out, *wavenc, *filesink;

    //Audio stream out (sender) plugins
    filesrc         = gst_element_factory_make("filesrc",       NULL);
    wavparse        = gst_element_factory_make("wavparse",      NULL);
    audioresample   = gst_element_factory_make("audioresample", NULL);
    audioconvert_in = gst_element_factory_make("audioconvert", NULL);
    rtpamrpay       = gst_element_factory_make("rtpamrpay",     NULL);

    //Codec dependant elements
    if (usingAmrNb){
        printf("GST: Using narrow band codecs amrnbenc and amrnbdec \n");
        amrencoder = gst_element_factory_make("amrnbenc", NULL);
        amrdecoder = gst_element_factory_make("amrnbdec", NULL);
    }
    else{
        printf("GST: Using wideband codecs voamrwbenc and amrwbdec \n");
        amrencoder = gst_element_factory_make("voamrwbenc", NULL);
        amrdecoder = gst_element_factory_make("amrwbdec",   NULL);
    }

    g_object_set(amrencoder, "band-mode", 8, NULL);

    //Socket plugins
    rtpbin           = gst_element_factory_make("rtpbin",  "rtpbin");
    hrtpsink         = gst_element_factory_make("udpsink", "hrtpsink");
    hrtcpsink        = gst_element_factory_make("udpsink", "hrtcpsink");
    receiver_rtcp_in = gst_element_factory_make("udpsrc",  "receiver_rtcp_in");
    h264recv         = gst_element_factory_make("udpsrc",  "h264recv");


    //Audio stream in (receiver) plugins
    rtpamrdepay      = gst_element_factory_make("rtpamrdepay",  NULL);
    audioconvert_out = gst_element_factory_make("audioconvert", NULL);
    wavenc           = gst_element_factory_make("wavenc",       NULL);
    filesink         = gst_element_factory_make("filesink",     NULL);

    g_object_set(filesrc,  "location", FILENAME,     NULL);
    g_object_set(filesink, "location", RECV_FILENAME, NULL);

    //GST: Set payload size as negotiated
    //g_object_set(rtpamrpay, "pt", 96, NULL);

    //g_object_set(rtpamrpay, "octet-align",   1, NULL);
    //g_object_set(rtpamrdepay, "octet-align", 1, NULL);

    printf("GST: receiver_rtcp_in port %d address %s\n", RTCP_REMOTE_PORT, LOCAL_IP);
    g_object_set(receiver_rtcp_in, "port", RTCP_REMOTE_PORT, "address", LOCAL_IP, NULL);

    GstCaps     *audio_caps;
    audio_caps = gst_caps_new_simple("application/x-rtp",
        "media", G_TYPE_STRING, "audio",
        "payload", G_TYPE_INT, 96,
        "clock-rate", G_TYPE_INT, 16000,
        "encoding-name", G_TYPE_STRING, "AMR-WB",
        "encoding-params", G_TYPE_STRING, "1",
	"octet-align", G_TYPE_STRING, "1",
        NULL);

    printf("GST: rtp port %d address %s\n", RTP_REMOTE_PORT, LOCAL_IP);
    g_object_set(h264recv, "port", RTP_REMOTE_PORT, "address", LOCAL_IP, "caps", audio_caps, NULL);


    
    GSocket* socket_gst = NULL;
    g_object_get(h264recv, "socket",     &socket_gst, NULL);
    g_object_set(hrtpsink, "socket",     socket_gst, NULL);


    
    socket_gst = NULL;
    g_object_get(receiver_rtcp_in, "socket", &socket_gst, NULL);
    g_object_set(hrtcpsink, "socket", socket_gst, NULL);
   

    
    printf("Fijamos socket de hrtpsink\n");
    fflush(stdout);
//    printf("GST: hrtpsink -> socket parameter set\n");
    printf("Fijamos socket de h264recv\n");
    fflush(stdout);
//    printf("GST: h264recv -> socket parameter set\n");

    printf("Fijamos async de hrtpsink\n");
    fflush(stdout);
//    printf("GST: hrtpsink -> async parameter set\n");
    g_object_set(hrtpsink, "async", FALSE, NULL);
    printf("Fijamos port de hrtpsink:%d \n", RTP_REMOTE_PORT);
    fflush(stdout);
//    printf("GST: hrtpsink -> port parameter set to %d \n", RTP_REMOTE_PORT);
    g_object_set(hrtpsink, "port", RTP_REMOTE_PORT, NULL);
    printf("Fijamos host de hrtpsink:%s \n", LOCAL_IP);
    fflush(stdout);
//    printf("GST: hrtpsink -> host parameter set to %s \n", LOCAL_IP);
    g_object_set(hrtpsink, "host", LOCAL_IP, NULL);
    printf("Fijamos ts-offset de hrtpsink\n");
    fflush(stdout);
//    printf("GST: hrtpsink -> ts-offset parameter set \n");
    g_object_set(hrtpsink, "ts-offset", 0, NULL);
    
    printf("Fijamos socket de hrtcpsink\n");
    fflush(stdout);
    printf("Fijamos port de hrtcpsink:%d \n", RTCP_REMOTE_PORT);
    fflush(stdout);
 //   printf("GST: hrtcpsink -> port parameter set to %d \n", RTCP_REMOTE_PORT);
    g_object_set(hrtcpsink, "port", RTCP_REMOTE_PORT, NULL);
    printf("Fijamos host de hrtcpsink:%s \n", LOCAL_IP);
    fflush(stdout);
 //   printf("GST: hrtcpsink -> host parameter set to %s \n", LOCAL_IP);
    g_object_set(hrtcpsink, "host", LOCAL_IP, NULL);
    printf("Fijamos sync de hrtcpsink\n");
    fflush(stdout);
 //   printf("GST: hrtcpsink -> sync parameter set\n");
    g_object_set(hrtcpsink, "sync", FALSE, NULL);
    printf("Fijamos async de hrtcpsink\n");
    fflush(stdout);
 //   printf("GST: hrtcpsink -> async parameter set\n");
    g_object_set(hrtcpsink, "async", FALSE, NULL);

  
    
  


    pipeline = GST_PIPELINE(gst_pipeline_new(NULL));
    gst_bin_add_many(GST_BIN(pipeline), rtpbin, filesrc, wavparse, audioresample, amrencoder, rtpamrpay, hrtpsink, hrtcpsink, NULL);

    if (gst_element_link_many(filesrc, wavparse, audioresample, amrencoder, rtpamrpay, NULL) != TRUE)
        printf("GST: Problem linking sender part!!!!!!!\n");

    gst_element_link_pads_filtered(rtpamrpay, "src", rtpbin, "send_rtp_sink_0", NULL);
    gst_element_link_pads_filtered(rtpbin, "send_rtp_src_0", hrtpsink, "sink", NULL);
    gst_element_link_pads_filtered(rtpbin, "send_rtcp_src_0", hrtcpsink, "sink", NULL);

    gst_bin_add_many(GST_BIN(pipeline), receiver_rtcp_in, h264recv, rtpamrdepay, amrdecoder, audioconvert_out, wavenc, filesink, NULL);
    if (gst_element_link_many(h264recv, rtpamrdepay, amrdecoder, audioconvert_out, wavenc, filesink, NULL) != TRUE)
    {
        printf("GST: problem linking elements\n");
    }

    gst_element_link_pads_filtered(rtpbin, "recv_rtp_src_0", rtpamrdepay, "sink", NULL);
    gst_element_link_pads_filtered(h264recv, "src", rtpbin, "recv_rtp_sink_0", NULL);
    gst_element_link_pads_filtered(receiver_rtcp_in, "src", rtpbin, "recv_rtcp_sink_0", NULL);

    g_signal_connect(rtpbin, "pad-removed", G_CALLBACK(pad_removed_cb), NULL);

    g_signal_connect(rtpbin, "on-ssrc-active", G_CALLBACK(on_ssrc_active_cb), rtpamrdepay);
    g_signal_connect(rtpbin, "on-sender-timeout", G_CALLBACK(timeout_callback), (gpointer) GST_ON_SENDER_TIMEOUT);
    g_signal_connect(rtpbin, "on-timeout", G_CALLBACK(timeout_callback), (gpointer) GST_ON_TIMEOUT);
    g_signal_connect(rtpbin, "on-bye-ssrc", G_CALLBACK(timeout_callback), (gpointer) GST_ON_BYE_SSRC);
    g_signal_connect(rtpbin, "on-bye-timeout", G_CALLBACK(timeout_callback), (gpointer) GST_ON_BYE_TIME_OUT);

    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);

    GstBus  *bus = gst_element_get_bus(GST_ELEMENT(pipeline));
    
    GstMessage *msg = gst_bus_timed_pop_filtered(bus, 6000* (unsigned long long int)3100000, GST_MESSAGE_EOS);


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
        source = g_value_get_object(val);

        GstStructure *stats;

        g_object_get(source, "stats", &stats, NULL);

        /* simply dump the stats structure */
        gchar *str;
        str = gst_structure_to_string(stats);
        g_print("source stats: %s\n", str);
        printf("source stats: %s\n", str);
        g_free(str);

        gst_structure_free(stats);
    }
    
    for (i = 0; i < arr->n_values; i++)
    {
        GValue *value = arr->values + i;

        if (G_VALUE_TYPE(value) != 0)   // we allow unset values in the array
            g_value_unset(value);
    }


    
    
    
    return 0;
}


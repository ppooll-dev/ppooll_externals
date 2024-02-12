/*
 ll_mcwaveform
 by klaus filip 2020
 read, file, drag&drop
 by joe stecatto 2024
 */

#ifdef WIN_VERSION
#define _CRT_SECURE_NO_DEPRECATE
#include <float.h>
#endif

#include "ext.h"
#include "ext_obex.h"
#include "ext_common.h"
#include "ext_expr.h"
#include "ext_obex.h" //for atom routines
#include "jpatcher_api.h"
#include "jgraphics.h"
#include "ext_parameter.h"
//#include "common/commonsyms.c"
#include "ext_systhread.h"
#include "ext_buffer.h"
#include "z_dsp.h"
#include <stdio.h>
#include <math.h>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

static t_class  *s_ll_mcwaveform_class = 0;
static t_pt s_ll_mcwaveform_cum; // mouse tracking
static t_pt s_ll_ccum;
static t_pt s_ll_delta;

typedef enum {
    MOUSE_MODE_NONE,
    MOUSE_MODE_SELECT,
    MOUSE_MODE_LOOP,
    MOUSE_MODE_MOVE,
    MOUSE_MODE_DRAW
} MouseMode;

typedef struct {
    double start;
    double length;
    double sel_start;
    double sel_end;
} MSList;

t_fourcc audiotypes[] = {'AIFF', 'WAVE', 'MP3 ', 'FLAC', 'M4A '};
short naudiotypes = sizeof(audiotypes) / sizeof(t_fourcc);

typedef struct _ll_mcwaveform
{
    t_jbox      ll_box;
    double      ll_width;
    double      ll_height;
    long        ll_rectsize;
    t_jrgba     ll_wfcolor;
    t_jrgba     ll_selcolor;
    t_jrgba     ll_bgcolor;
    t_jrgba     ll_linecolor;
    char        inv_sel_color;

    char        mouse_mode; // none, select, loop, move, draw(?)
    short       mouse_down; // bool
    short       mouse_over; // bool

    short       allow_drag; // bool

    short       sf_mode;    // bool, is soundfile mode
    short       sf_read;    // bool, needs to reread values from file for drawing

    short       wf_paint;   // bool, should repaint the waveform

    t_buffer_ref *l_buffer_reference;
    t_symbol     *bufname;
    short        buf_found;

    long        chans;
    long        chan_offset;
    long        chans_old;
    long        chan_offset_old;

    // Source Buffer/File Properties
    long        l_chan;         // Total channels
    double      l_srms;         // Samplerate (ms)
    double      l_length;       // Length (ms)
    unsigned long int l_frames; // Total frames (samples)

    t_float     buf_arr[10000][30];

    MSList      ms_list;    // start, display, sel_start, sel_end
    double      msold[2];   // last redrawn values for start & display

    double      linepos;    // Line Position (ms)

    double      vzoom;      // vertical zoom attribute value
    double      vzoom_adj;  // vertical zoom scaled value (to use for painting)


    t_object    *buffer;    // buffer~
    t_atom      *path;      // Path to the sound file source

    t_atom      msg[4], rv; // Dummy atom array, still being use but should be replaced

    // Buffer refresh clock
    t_qelem*    m_qelem;
    t_clock*    m_clock;
    long        should_reread; //
    double      reread_rate;   // Max refresh rate in ms


    // ?? (I think we can remove, we only set it, never use it...)
    double      l_arr_start;
    double      l_arr_len;

} t_ll_mcwaveform;

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ basic
void *ll_mcwaveform_new(t_symbol *s, short argc, t_atom *argv);
void ll_mcwaveform_assist(t_ll_mcwaveform *x, void *b, long m, long a, char *s);
void ll_mcwaveform_free(t_ll_mcwaveform *x);
void ll_mcwaveform_qtask(t_ll_mcwaveform *x, t_symbol *s, short argc, t_atom *argv);
void ll_mcwaveform_task(t_ll_mcwaveform *x);
t_max_err ll_mcwaveform_notify(t_ll_mcwaveform *x, t_symbol *s, t_symbol *msg, void *sender, void *data);
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ buffer
void ll_mcwaveform_set(t_ll_mcwaveform *x, t_symbol *s);
void ll_mcwaveform_file(t_ll_mcwaveform *x, t_symbol *s);
void ll_mcwaveform_sf(t_ll_mcwaveform *x, t_symbol *s, long ac, t_atom *av);
void ll_mcwaveform_read(t_ll_mcwaveform *x, t_symbol *s);
void ll_mcwaveform_doread(t_ll_mcwaveform *x, t_symbol *s);
long ll_mcwaveform_acceptsdrag_unlocked(t_ll_mcwaveform *x, t_object *drag, t_object *view);
long ll_mcwaveform_acceptsdrag_locked(t_ll_mcwaveform *x, t_object *drag, t_object *view);
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ attribute accessors
t_max_err ll_mcwaveform_vzoom_set(t_ll_mcwaveform *x, void *attr, long ac, t_atom *av);

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ input types
void ll_mcwaveform_bang(t_ll_mcwaveform *x);
void ll_mcwaveform_int(t_ll_mcwaveform *x, long n);
void ll_mcwaveform_float(t_ll_mcwaveform *x, double f);
void ll_mcwaveform_list(t_ll_mcwaveform *x, t_symbol *s, short ac, t_atom *av);

void ll_mcwaveform_mode(t_ll_mcwaveform *x, t_symbol *s, long ac, t_atom *av);
void ll_mcwaveform_chans(t_ll_mcwaveform *x, t_symbol *s, long ac, t_atom *av);
void ll_mcwaveform_line(t_ll_mcwaveform *x, double f);

void ll_mcwaveform_start(t_ll_mcwaveform *x, double f);
void ll_mcwaveform_length(t_ll_mcwaveform *x, double f);
void ll_mcwaveform_selstart(t_ll_mcwaveform *x, double f);
void ll_mcwaveform_selend(t_ll_mcwaveform *x, double f);

void ll_mcwaveform_full(t_ll_mcwaveform *x);
void ll_mcwaveform_zoom2sel(t_ll_mcwaveform *x);
void ll_mcwaveform_sel_all(t_ll_mcwaveform *x);
void ll_mcwaveform_sel_disp(t_ll_mcwaveform *x);
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ paint
void ll_mcwaveform_reread(t_ll_mcwaveform *x);
void ll_mcwaveform_paint(t_ll_mcwaveform *x, t_object *view);
void ll_mcwaveform_paint_wf(t_ll_mcwaveform *x, t_object *view, t_rect *rect);
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ mouse &key
void ll_mcwaveform_setmousecursor(t_ll_mcwaveform *x, t_object *patcherview, t_pt pt, long modifiers);
void ll_mcwaveform_mouseenter(t_ll_mcwaveform *x, t_object *patcherview, t_pt pt, long modifiers);
void ll_mcwaveform_mouseleave(t_ll_mcwaveform *x, t_object *patcherview, t_pt pt, long modifiers);
void ll_mcwaveform_mousedown(t_ll_mcwaveform *x, t_object *patcherview, t_pt pt, long modifiers);
void ll_mcwaveform_mousedrag(t_ll_mcwaveform *x, t_object *patcherview, t_pt pt, long modifiers);
void ll_mcwaveform_mouseup(t_ll_mcwaveform *x, t_object *patcherview, t_pt pt, long modifiers);
void ll_mcwaveform_mousemove(t_ll_mcwaveform *x, t_object *patcherview, t_pt pt, long modifiers);
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ basic
void ext_main(void *r){
    t_class *c;
    common_symbols_init();
    c = class_new("ll_mcwaveform",
                  (method)ll_mcwaveform_new,
                  (method)ll_mcwaveform_free,
                  sizeof(t_ll_mcwaveform),
                  (method)NULL,
                  A_GIMME,
                  0L
    );
    
    c->c_flags |= CLASS_FLAG_NEWDICTIONARY; // to specify dictionary constructor
    
    jbox_initclass(c, JBOX_COLOR );
    //jbox_initclass(c, JBOX_TEXTFIELD | JBOX_FIXWIDTH | JBOX_COLOR | JBOX_FONTATTR);
    
    class_addmethod(c, (method)ll_mcwaveform_paint,         "paint",      A_CANT, 0);
    class_addmethod(c, (method)ll_mcwaveform_int,           "int",        A_LONG, 0);
    class_addmethod(c, (method)ll_mcwaveform_float,         "float",      A_FLOAT, 0);
    class_addmethod(c, (method)ll_mcwaveform_list,          "list",       A_GIMME, 0);
    class_addmethod(c, (method)ll_mcwaveform_bang,          "bang",       0);

    class_addmethod(c, (method)ll_mcwaveform_mode,          "mode",       A_GIMME, 0);
    class_addmethod(c, (method)ll_mcwaveform_chans,         "chans",      A_GIMME, 0);
    class_addmethod(c, (method)ll_mcwaveform_line,          "line",       A_FLOAT, 0);

    class_addmethod(c, (method)ll_mcwaveform_start,         "start",      A_FLOAT, 0);
    class_addmethod(c, (method)ll_mcwaveform_length,        "length",     A_FLOAT, 0);
    class_addmethod(c, (method)ll_mcwaveform_selstart,      "selstart",   A_FLOAT, 0);
    class_addmethod(c, (method)ll_mcwaveform_selend,        "selend",     A_FLOAT, 0);

    class_addmethod(c, (method)ll_mcwaveform_zoom2sel,      "zoom2sel",   0);
    class_addmethod(c, (method)ll_mcwaveform_sel_all,       "sel_all",    0);
    class_addmethod(c, (method)ll_mcwaveform_sel_disp,      "sel_disp",   0);
    class_addmethod(c, (method)ll_mcwaveform_full,          "full",       0);
    class_addmethod(c, (method)ll_mcwaveform_reread,        "reread",     0);

    class_addmethod(c, (method)ll_mcwaveform_notify,        "notify",     A_CANT, 0);
    
    class_addmethod(c, (method) ll_mcwaveform_mousemove,    "mousemove",  A_CANT, 0);
    class_addmethod(c, (method)ll_mcwaveform_mousedown,     "mousedown",  A_CANT, 0);
    class_addmethod(c, (method)ll_mcwaveform_mousedrag,     "mousedrag",  A_CANT, 0);
    class_addmethod(c, (method)ll_mcwaveform_mouseup,       "mouseup",    A_CANT, 0);
    class_addmethod(c, (method)ll_mcwaveform_mouseenter,    "mouseenter", A_CANT, 0);

    class_addmethod(c, (method)ll_mcwaveform_assist,        "assist",     A_CANT, 0);

    class_addmethod(c, (method)ll_mcwaveform_read,          "read",       A_DEFSYM, 0);
    class_addmethod(c, (method)ll_mcwaveform_set,           "set",        A_SYM, 0);
    class_addmethod(c, (method)ll_mcwaveform_sf,            "sf",         A_GIMME, 0);

    class_addmethod(c, (method)ll_mcwaveform_acceptsdrag_locked,    "acceptsdrag_locked",   A_CANT, 0);
    class_addmethod(c, (method)ll_mcwaveform_acceptsdrag_unlocked,  "acceptsdrag_unlocked", A_CANT, 0);
    
    CLASS_ATTR_DEFAULT(c,"patching_rect",0, "0. 0. 200. 100.");

    //**********fonts
    //CLASS_STICKY_ATTR(c,"category",0,"Font");
    //CLASS_ATTR_DEFAULTNAME_SAVE_PAINT(c,"fontname",0,"Arial");
    //CLASS_ATTR_DEFAULTNAME_SAVE_PAINT(c,"fontsize",0,"12");

    //**********behavior
    CLASS_STICKY_ATTR(c, "category", 0, "Appearance");

    CLASS_ATTR_DOUBLE(c,                "vzoom", 0, t_ll_mcwaveform, vzoom);
    CLASS_ATTR_LABEL(c,                 "vzoom", 0, "Vertical Zoom");
    CLASS_ATTR_FILTER_CLIP(c,           "vzoom", 0., 1.);
    CLASS_ATTR_DEFAULT_SAVE(c,          "vzoom", 0, "1");
    CLASS_ATTR_ACCESSORS(c,             "vzoom", NULL, (method)ll_mcwaveform_vzoom_set);

    CLASS_STICKY_ATTR(c, "category", 0, "Behavior");


    CLASS_ATTR_CHAR(c,                  "setmode", 0, t_ll_mcwaveform, mouse_mode);
    CLASS_ATTR_STYLE_LABEL(c,           "setmode", 0, "enum", "Click Mode");
    CLASS_ATTR_ENUMINDEX(c,             "setmode", 0, "none select loop move");
    CLASS_ATTR_DEFAULT_SAVE_PAINT(c,    "setmode", 0, "1");

    CLASS_ATTR_CHAR(c,                  "allowdrag", 0, t_ll_mcwaveform, allow_drag);
    CLASS_ATTR_STYLE(c,                 "allowdrag", 0, "onoff");
    CLASS_ATTR_STYLE_LABEL(c,           "allowdrag", 0, "onoff", "Allow Drag & Drop");
    CLASS_ATTR_DEFAULT_SAVE(c,          "allowdrag", 0, "1");

    // CLASS_ATTR_LONG(c,                  "chans", 0, t_ll_mcwaveform, chans);
    // CLASS_ATTR_LABEL(c,                 "chans", 0, "Channels");
    // CLASS_ATTR_MIN(c,                   "chans", 0, "0");
    // CLASS_ATTR_FILTER_MIN(c,            "chans", 0);
    // CLASS_ATTR_DEFAULT_SAVE_PAINT(c,    "chans", 0, "1");

    // CLASS_ATTR_LONG(c,                  "chanoffset", 0, t_ll_mcwaveform, chan_offset);
    // CLASS_ATTR_LABEL(c,                 "chanoffset", 0, "Channel Offset");
    // CLASS_ATTR_MIN(c,                   "chanoffset", 0, "0");
    // CLASS_ATTR_FILTER_MIN(c,            "chanoffset", 0);
    // CLASS_ATTR_DEFAULT_SAVE_PAINT(c,    "chanoffset", 0, "0");

    CLASS_ATTR_DOUBLE(c,                "reread_rate", 0, t_ll_mcwaveform, reread_rate);
    CLASS_ATTR_LABEL(c,                 "reread_rate", 0, "Re-read rate (ms)");
    CLASS_ATTR_FILTER_CLIP(c,           "reread_rate", 1., 2000.);
    CLASS_ATTR_DEFAULT_SAVE(c,          "reread_rate", 0, "500");

    //**********colors
    CLASS_STICKY_ATTR(c, "category", 0, "Color");

    CLASS_ATTR_RGBA_LEGACY(c,           "wfcolor", "frgb", 0, t_ll_mcwaveform, ll_wfcolor);
    CLASS_ATTR_DEFAULTNAME_SAVE_PAINT(c,"wfcolor", 0, "0.55 0.2 0.84 1.");
    CLASS_ATTR_STYLE_LABEL(c,           "wfcolor", 0, "rgba" ,"Waveform Color");
    
    CLASS_ATTR_RGBA_LEGACY(c,           "selcolor", "rgb2", 0, t_ll_mcwaveform, ll_selcolor);
    CLASS_ATTR_DEFAULT_SAVE_PAINT(c,    "selcolor", 0, "0.24 0.5 0.8 0.55");
    CLASS_ATTR_STYLE_LABEL(c,           "selcolor", 0, "rgba", "Select Color");
    
    CLASS_ATTR_RGBA_LEGACY(c,           "bgcolor", "brgb", 0, t_ll_mcwaveform, ll_bgcolor);
    CLASS_ATTR_DEFAULTNAME_SAVE_PAINT(c,"bgcolor", 0, "0.8 0.8 0.8 1.");
    CLASS_ATTR_STYLE_LABEL(c,           "bgcolor", 0, "rgba", "Background Color");
    
    CLASS_ATTR_RGBA_LEGACY(c,           "linecolor", "rgb3", 0, t_ll_mcwaveform, ll_linecolor);
    CLASS_ATTR_DEFAULTNAME_SAVE_PAINT(c,"linecolor", 0, "0. 0. 0. 1.");
    CLASS_ATTR_STYLE_LABEL(c,           "linecolor", 0, "rgba", "Line Color");
    
    CLASS_ATTR_CHAR(c,                  "inv_sel_color", 0, t_ll_mcwaveform, inv_sel_color);
    CLASS_ATTR_STYLE_LABEL(c,           "inv_sel_color", 0, "onoff", "Invert Selection Color");
    CLASS_ATTR_DEFAULT_SAVE_PAINT(c,    "inv_sel_color", 0, "0");
    
    CLASS_STICKY_ATTR_CLEAR(c,          "category");

    // Set Order
    CLASS_ATTR_ORDER(c, "wfcolor",      0, "1");
    CLASS_ATTR_ORDER(c, "selcolor",     0, "2");
    CLASS_ATTR_ORDER(c, "bgcolor",      0, "3");
    CLASS_ATTR_ORDER(c, "linecolor",    0, "4");

    CLASS_ATTR_INVISIBLE(c, "color", 0);
    CLASS_ATTR_ATTR_PARSE(c, "color","save", USESYM(long), 0, "0");

    class_register(CLASS_BOX, c);
    s_ll_mcwaveform_class = c;
}

void *ll_mcwaveform_new(t_symbol *s, short argc, t_atom *argv){
    t_ll_mcwaveform* x;
    long flags;
    t_dictionary *d=NULL;
    t_rect rect;
    t_jgraphics *g;
    t_object *view = NULL;
    
    if (!(d=object_dictionaryarg(argc,argv)))
        return NULL;
    
    x = (t_ll_mcwaveform *) object_alloc(s_ll_mcwaveform_class);
    if (!x)
        return NULL;
    
    flags = 0
    | JBOX_DRAWFIRSTIN
    | JBOX_NODRAWBOX
    | JBOX_DRAWINLAST           // rbs -- I think we can turn this off
    //      | JBOX_TRANSPARENT
    //      | JBOX_NOGROW
    //      | JBOX_GROWY
    | JBOX_GROWBOTH
    | JBOX_HILITE
    | JBOX_DRAWBACKGROUND
    //      | JBOX_MOUSEDRAGDELTA
    //      | JBOX_NOFLOATINSPECTOR
    //      | JBOX_TEXTFIELD
    ;
    
    jbox_new(&x->ll_box, flags, argc, argv);
    x->ll_box.b_firstin = (t_object*) x;
    outlet_new((t_object *)x,NULL);
   
    attr_dictionary_process(x,d); // handle attribute args
    jbox_ready(&x->ll_box);
    
    g = (t_jgraphics*) patcherview_get_jgraphics(view);
    jbox_get_rect_for_view((t_object *)x, view, &rect);

    x->ll_width = rect.width - x->ll_rectsize;
    x->ll_height = rect.height - x->ll_rectsize;

    x->ms_list.start = 0.;
    x->ms_list.length = 0.;
    x->ms_list.sel_start = 0.;
    x->ms_list.sel_end = 0.;

    x->msold[0] = -1;
    x->msold[1] = -1;

    x->chans = 0;
    x->chan_offset = 0;
    x->chans_old = 0;
    x->chan_offset_old = 0;

    x->sf_mode = 0;
    x->sf_read = 0;

    x->vzoom = 1.0;
    x->linepos = -1.;

    x->should_reread = 0;

    // create placeholder buffer for soundfile
    t_symbol *unique_id = symbol_unique();
    t_atom buf_msg[1];
    atom_setsym(buf_msg, unique_id);
    t_object *newBuf = object_new_typed(CLASS_BOX, gensym("buffer~"), 1, buf_msg);
    x->buffer = jbox_get_object(newBuf);

    if(!x->buffer){
        object_error((t_object *)x, "Error creating internal buffer.");
    }

    x->bufname = unique_id;

    x->m_clock = clock_new((t_ll_mcwaveform *)x, (method)ll_mcwaveform_task);
    x->m_qelem = qelem_new((t_ll_mcwaveform *)x, (method)ll_mcwaveform_qtask);
    return x;
}

void ll_mcwaveform_assist(t_ll_mcwaveform *x, void *b, long m, long a, char *s){
    if (m == ASSIST_INLET)
        sprintf(s,"list");
    else
        sprintf(s,"list: start, length, selstart, selend");
}

void ll_mcwaveform_free(t_ll_mcwaveform *x){
    object_free(x->l_buffer_reference);
    object_free(x->buffer);

    jbox_free(&x->ll_box);
    clock_free(x->m_clock);
    qelem_free(x->m_qelem);
}

void ll_mcwaveform_qtask(t_ll_mcwaveform *x, t_symbol *s, short argc, t_atom *argv){
    ll_mcwaveform_reread(x);
}

void ll_mcwaveform_task(t_ll_mcwaveform *x){
    x->should_reread = 0;
}

t_max_err ll_mcwaveform_notify(t_ll_mcwaveform *x, t_symbol *s, t_symbol *msg, void *sender, void *data){
    // post("notification from %s: %s",s->s_name, msg->s_name);
    if(msg == gensym("globalsymbol_unbinding") || msg == gensym("globalsymbol_binding")){
        if(msg == gensym("globalsymbol_unbinding")){
            // post("buffer removed?");  // Buffer removed
            x->wf_paint = 1;
            x->sf_read = 0;
            x->should_reread = 0;

            x->msold[0]=0.;
            x->msold[1]=0.;

            x->ms_list.start = 0;
            x->ms_list.length = 0;
            x->ms_list.sel_start = 0;
            x->ms_list.sel_end = 0;

            x->linepos = -1;

            x->chans = 0;
            x->chan_offset = 0;

            x->chans_old = 0;
            x->chan_offset_old = 0;
            x->sf_mode = 0;

            ll_mcwaveform_bang(x);
        }else{
            // post("buffer added?");  // Buffer added
            ll_mcwaveform_reread(x);
        }
    }else if(msg == gensym("buffer_modified")){
        // "should_reread" flag -- reread only every x ms (reread_rate)
        if(!x->should_reread){
            x->should_reread = 1;

            qelem_set(x->m_qelem);
            clock_fdelay(x->m_clock, x->reread_rate); // Schedule the first tick
        }
        // post("buffer modified?");
    // }else if(msg == gensym("attr_changed")){
    //  // buffer set?
    }
    return buffer_ref_notify(x->l_buffer_reference, s, msg, sender, data);
}


/**************************************************************************************************
    
    Buffer & File Methods
        (drag&drop, read, doread, set, file, sf)

*************************************************************************************************/

/*
    acceptsdrag_unlocked
        Called when user drags file over object (unlocked).
        If audio file match, attempt to read into buffer.
        TO-DO: replace buffer if buffer_mode?
*/
long ll_mcwaveform_acceptsdrag_unlocked(t_ll_mcwaveform *x, t_object *drag, t_object *view){
    if(!x->allow_drag){
        return false;
    }
    if (jdrag_matchdragrole(drag, gensym("audiofile"), 0)) {
        jdrag_box_add(drag, (t_object *)x, gensym("read"));
        return true;
    }
    return false;
};

/*
    acceptsdrag_locked
        Called when user drags file over object (locked).
        Call acceptsdrag_unlocked -- same functionality.
*/
long ll_mcwaveform_acceptsdrag_locked(t_ll_mcwaveform *x, t_object *drag, t_object *view){
    return ll_mcwaveform_acceptsdrag_unlocked(x, drag, view);
};

/*
    read
        Imitates Max default "read" messages.
        Can include filename/path as second argument.
        Without it, use Open Dialog for selection.
*/
void ll_mcwaveform_read(t_ll_mcwaveform *x, t_symbol *s){
    defer(x, (method)ll_mcwaveform_doread, s, 0, NULL);
}

/*
    file
        Set the waveform by filename.
        This method uses the miniaudio decoder to parse the audio file header.
*/
void ll_mcwaveform_doread(t_ll_mcwaveform *x, t_symbol *s){
    t_fourcc typechosen; // This points to the selected file's type after the dialog
    
    char filename[MAX_PATH_CHARS];
    char fullpath[MAX_PATH_CHARS];

    short path;

    t_symbol *filepath_symbol;
 
    if (s != gensym("")) {
        ll_mcwaveform_file(x, s);
        return;
    }

    // If no filename/path argument, select file with Open Dialog
    if (open_dialog(filename, &path, &typechosen, &audiotypes, &naudiotypes)) // non-zero: user cancelled
        return;

    // Convert Path ID and filename to a fully qualified pathname
    if (path_topathname(path, filename, fullpath) != 0) {
        // Handle error if path cannot be converted
        object_error((t_object *)x, "Error converting to full path for file: %s", filename);
        return;
    }
    // Convert the fullpath to a symbol for further use
    filepath_symbol = gensym(fullpath);
    ll_mcwaveform_file(x, gensym(fullpath));
}

/*
    set
        Set the waveform by buffer~ name
*/
void ll_mcwaveform_set(t_ll_mcwaveform *x, t_symbol *s){
    t_float *tab;
    x->sf_mode = 0;

    if (!x->l_buffer_reference)
        x->l_buffer_reference = buffer_ref_new((t_object *)x, s);
    else
        buffer_ref_set(x->l_buffer_reference, s);
    
    t_buffer_obj *buffer = buffer_ref_getobject(x->l_buffer_reference);
    x->l_chan = buffer_getchannelcount(buffer);
    x->l_frames = buffer_getframecount(buffer);
    x->l_srms = buffer_getmillisamplerate(buffer);
    x->l_length = x->l_frames / x->l_srms;

    x->ms_list.start = 0;
    x->ms_list.length = x->l_length;
    x->wf_paint = 1;
    //post("frames %d channels %d srms %lf length %lf",x->l_frames, x->l_chan, x->l_srms, x->l_length);
    ll_mcwaveform_bang(x);
}

/*
    file
        Set the waveform by filename (if in Max search path) or full filepath (in Max format & style).
        This method uses the miniaudio decoder to parse the audio file header.
*/
void ll_mcwaveform_file(t_ll_mcwaveform *x, t_symbol *s){
    t_float *tab;
    x->sf_mode = 1;

    t_symbol *full_path = NULL; // This will hold the resolved full path (Max Format)
    t_max_err err = path_absolutepath(&full_path, s, audiotypes, naudiotypes);
    if (err != MAX_ERR_NONE) {
        object_error((t_object *)x, "Error resolving absolute path: \n%s",s->s_name);
        return; // Make sure to exit if there's an error
    }
    // Convert the Max path to a native system path in absolute form
    char filePath[MAX_PATH_CHARS]; // This will hold the POSIX path
    if (path_nameconform(full_path->s_name, filePath, PATH_STYLE_MAX, PATH_TYPE_BOOT) != MAX_ERR_NONE) {
        object_error((t_object *)x, "Error converting path to POSIX format: \n%s",full_path->s_name);
        return;
    }
    // Initialize the decoder to read the file's properties
    ma_decoder decoder;
    ma_result result;
    result = ma_decoder_init_file(filePath, NULL, &decoder);
    if (result != MA_SUCCESS) {
        object_error((t_object *)x, "Failed to initialize decoder for file '%s'.", filePath);
        return; // Adjust return type or handling based on your function's design
    }
    ma_uint64 totalFrames = 0; // Declare a variable to hold the total frames
    ma_result frameResult;

    // Call the function with the correct arguments
    frameResult = ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);
    if (frameResult != MA_SUCCESS) {
        object_error((t_object *)x, "Failed to get length in PCM frames: \n%s",full_path->s_name);
        ma_decoder_uninit(&decoder); // Make sure to uninitialize the decoder to avoid resource leaks
        return; // Adjust based on your function's return type and error handling strategy
    }

    // Now, you can use totalFrames as it contains the total number of PCM frames
    double lengthInMs = totalFrames / (double)decoder.outputSampleRate * 1000;
    
    // Clean up
    ma_decoder_uninit(&decoder);

    // Set reference to buffer -- is this already done?
    if (!x->l_buffer_reference){
        x->l_buffer_reference = buffer_ref_new((t_object *)x, x->bufname);
    }else{
        buffer_ref_set(x->l_buffer_reference, x->bufname);
    }

    // Set the sound file info (sfinfo)
    t_buffer_obj *buffer = buffer_ref_getobject(x->l_buffer_reference);
    x->l_chan = decoder.outputChannels;
    x->l_frames = totalFrames;
    x->l_srms = decoder.outputSampleRate / 1000;
    x->l_length = x->l_frames / x->l_srms;

    x->ms_list.start = 0;
    x->ms_list.length = x->l_length;

    // Output sfinfo
    t_atom sfinfo_list[4];
    atom_setsym(sfinfo_list, full_path);
    atom_setlong(sfinfo_list + 1, x->l_chan);
    atom_setlong(sfinfo_list + 2, decoder.outputSampleRate);
    double tempArray[1] = { x->ms_list.length };
    atom_setdouble_array(1, sfinfo_list + 3, 1, tempArray);
    outlet_anything(x->ll_box.b_ob.o_outlet, gensym("sf"), 4, sfinfo_list);

    // Init 600ms buffer for reading file in chunks
    atom_setlong(x->msg, 600);
    object_method_typed(x->buffer, gensym("sizeinsamps"), 1, x->msg, &x->rv);
    atom_setsym(x->msg, s);        // filename
    atom_setlong(x->msg + 1, 0);
    atom_setlong(x->msg + 2, 600);
    atom_setlong(x->msg + 3, x->l_chan);
    object_method_typed(x->buffer, gensym("read"), 4, x->msg, &x->rv);

    x->sf_read = 1;
    x->wf_paint = 1;
    ll_mcwaveform_bang(x);
}

/*
    sf
        sfplay mode -- load a filepath with required buffer info as additional args:
            [ path, channels, samplerate, length ]
*/
void ll_mcwaveform_sf(t_ll_mcwaveform *x, t_symbol *s, long ac, t_atom *av){
    long i;
    t_atom *ap;
    x->sf_mode = 1;

    // Parse list arguments of sfinfo (file, # channels, samplerate(ms), length(ms))
    for (i = 0, ap = av; i < ac; i++, ap++){
        switch (atom_gettype(ap)) {
            case A_LONG:
                if (i==1) x->l_chan = atom_getlong(ap);         // # chanels (ms)
                if (i==2) x->l_srms = atom_getlong(ap) * 0.001; // samplerate (ms)
                break;
            case A_FLOAT:
                if (i==3) x->l_length = atom_getfloat(ap);      // length (ms)
                break;
            case A_SYM:
                if (i==0) x->path = ap;                         // filename/path
                break;
            default:
                break;
        }
    }
    //process arguments
    x->ms_list.start=0;
    x->ms_list.length=x->l_length;

    atom_setlong(x->msg, 600);
    object_method_typed(x->buffer, gensym("sizeinsamps"), 1, x->msg, &x->rv);
    x->msg[0]= *x->path;
    atom_setlong(x->msg + 1, 0);
    atom_setlong(x->msg + 2, 600);
    atom_setlong(x->msg + 3, x->l_chan);
    object_method_typed(x->buffer, gensym("read"), 4, x->msg, &x->rv);
    
    if (!x->l_buffer_reference){
        x->l_buffer_reference = buffer_ref_new((t_object *)x, x->bufname);
    }else{
        buffer_ref_set(x->l_buffer_reference, x->bufname);
    }
    x->sf_read = 1;
    x->wf_paint = 1;
    ll_mcwaveform_bang(x);
}


/**************************************************************************************************
    
    Max Non-Message Inputs
        (bang, int, float, list)

*************************************************************************************************/

/*
    bang
        Check bounds for display and selection
        Redraw if necessary,
        Output list of display/selection values.
*/
void ll_mcwaveform_bang(t_ll_mcwaveform *x){
    // Ensure start is within [0, l_length - 0.5]
    x->ms_list.start = fmax(0, fmin(x->ms_list.start, x->l_length - 0.5));

    // Adjust end to not exceed l_length starting from start, and ensure a minimum value
    double maxEndValue = x->l_length - x->ms_list.start;
    double minEndValue = 20 / x->l_srms;
    x->ms_list.length = fmax(minEndValue, fmin(x->ms_list.length, maxEndValue));

    // Clamp sel_start and sel_end within the allowed range directly
    x->ms_list.sel_start = fmax(0, x->ms_list.sel_start);
    x->ms_list.sel_end = fmin(x->l_length, x->ms_list.sel_end);

    // Calls paint()
    jbox_redraw(&x->ll_box);

    double tempArray[4] = {
        x->ms_list.start,
        x->ms_list.length,
        x->ms_list.sel_start,
        x->ms_list.sel_end
    };

    t_atom myList[4];
    atom_setdouble_array(4, myList, 4, tempArray);
    outlet_anything(x->ll_box.b_ob.o_outlet, gensym("mslist"), 4, myList);
}

void ll_mcwaveform_int(t_ll_mcwaveform *x, long n){
    //post("int");
    // ll_mcwaveform_float(x, (double)n);
}

void ll_mcwaveform_float(t_ll_mcwaveform *x, double f){
    //post("float");
}

/*
    list [Max "list" Input]
        Set the start, length, sel_start and sel_end.
*/
void ll_mcwaveform_list(t_ll_mcwaveform *x, t_symbol *s, short ac, t_atom *av){
    //post("list");
    double tempArray[4];
    // Assuming ac is at least 4 and av points to an array of t_atom with at least 4 elements
    if (ac >= 4) {
        atom_getdouble_array(ac, av, 4, tempArray);

        // Assign the values from tempArray to the respective fields in ms_list
        x->ms_list.start = tempArray[0];
        x->ms_list.length = tempArray[1];
        x->ms_list.sel_start = tempArray[2];
        x->ms_list.sel_end = tempArray[3];
        //post("h %lf %lf %lf %lf", x->ms_list.start,x->ms_list.length,x->ms_list.sel_start,x->ms_list.sel_end);
    }
    ll_mcwaveform_bang(x);
}


/**************************************************************************************************
    
    Object-Specific Messages
        (mode, chans, vzoom, line, start, length, selstart, selend, zoom2sel, sel_all, sel_disp, full)

*************************************************************************************************/

/*
    "mode" [Max Message]

        Set the mouse mode with either a symbol or number
        Currently "draw" mode is ignored.
*/
void ll_mcwaveform_mode(t_ll_mcwaveform *x, t_symbol *s, long ac, t_atom *av) {
    // Check if there are arguments and proceed.
    if (ac && av) {
        long argType = atom_gettype(av);
        
        switch (argType) {
            case A_SYM: {
                const char *mode_str = atom_getsym(av)->s_name;
                
                // Using a series of if-else if statements to set the mouse mode.
                if (strcmp(mode_str, "none") == 0) {
                    x->mouse_mode = MOUSE_MODE_NONE;
                } else if (strcmp(mode_str, "select") == 0) {
                    x->mouse_mode = MOUSE_MODE_SELECT;
                } else if (strcmp(mode_str, "loop") == 0) {
                    x->mouse_mode = MOUSE_MODE_LOOP;
                } else if (strcmp(mode_str, "move") == 0) {
                    x->mouse_mode = MOUSE_MODE_MOVE;
                } else if (strcmp(mode_str, "draw") == 0) {
                    x->mouse_mode = MOUSE_MODE_DRAW;
                } else {
                    // Error handling for unsupported string values.
                    object_error((t_object *)x, "unsupported mode: %s", mode_str);
                    return;
                }
                break;
            }
            case A_LONG:
                // Direct assignment if the argument is a long.
                x->mouse_mode = (int)atom_getlong(av);
                break;
            case A_FLOAT:
                // Handling float arguments with an error.
                object_error((t_object *)x, "bad argument type for message 'mode': float not supported");
                break;
            default:
                // Handling other unexpected argument types with an error.
                object_error((t_object *)x, "bad argument type for message 'mode': ");
                return;
        }
    } else {
        // Handling cases where no valid arguments are provided.
        object_error((t_object *)x, "no arguments provided for message mode");
    }
}

/*
    "chans" [Max Message]
        Sets the number of channels to display, with the option for an offset channel index.

        symbol "all": show all channels
        number: show the first "n" channels
        list (2 numbers): show "n" channels offset by the 2nd number arg
*/
void ll_mcwaveform_chans(t_ll_mcwaveform *x, t_symbol *s, long ac, t_atom *av) {
    // Handle single argument case -- set number of channels to display.
    if (ac == 1 && av) {
        long argType = atom_gettype(av);
        switch (argType) {
            case A_SYM:
                // Check if the argument is "all" to set channels to l_chan, else error.
                if (strcmp(atom_getsym(av)->s_name, "all") == 0) {
                    x->chans = x->l_chan;
                } else {
                    object_error((t_object *)x, "bad argument for message chans");
                    return;
                }
                break;
            case A_LONG:
                // Set channels directly from the argument.
                x->chans = atom_getlong(av);
                break;
            case A_FLOAT:
                // Float arguments not supported for this message.
                object_error((t_object *)x, "bad argument for message chans");
                return;
            default:
                // Handle unexpected argument types.
                object_error((t_object *)x, "unexpected argument type for message chans");
                return;
        }
    }
    // Handle two arguments case -- set number of channels to display & channel offset.
    else if (ac == 2 && av && atom_gettype(&av[0]) == A_LONG && atom_gettype(&av[1]) == A_LONG) {
        x->chans = atom_getlong(&av[0]);
        x->chan_offset = atom_getlong(&av[1]);
    }
    else {
        // Handle invalid number of arguments or types.
        object_error((t_object *)x, "bad argument count or type for message chans");
        return;
    }

    // Validate and correct `chans` and `chan_offset` values.
    if (x->chans == 0) {
        x->chans = x->l_chan;
    }
    x->chans = fmaxl(1, fminl(x->chans, x->l_chan));
    x->chan_offset = fmaxl(0, fminl(x->chan_offset, x->l_chan - x->chans));

    // Re-read waveform data based on updated channel configuration.
    ll_mcwaveform_reread(x);
}

/*
    vzoom_set [Custom Attribute Accessor]
        we need this because we have to set the vzoom_adj & we need to manually call "reread"
*/
t_max_err ll_mcwaveform_vzoom_set(t_ll_mcwaveform *x, void *attr, long ac, t_atom *av){
    double f = 1.;
    if (ac&&av) {
        f = atom_getfloat(av);
    }
    x->vzoom = f;
    double new_zoom = f <= 0.000001 ? 0.000001 : f;
    x->vzoom_adj = pow(new_zoom, 0.003);
    ll_mcwaveform_reread(x);
    return MAX_ERR_NONE;
}

/*
    line [Max Message]
        Draw a line at the millisecond position.
*/
void ll_mcwaveform_line(t_ll_mcwaveform *x, double f){
    x->linepos = f < 0. ? -1. : f;
    ll_mcwaveform_bang(x);
}

/*
    start [Max Message]
        Set the display start position in ms.
*/
void ll_mcwaveform_start(t_ll_mcwaveform *x, double f){
    x->ms_list.start = f;
    ll_mcwaveform_bang(x);
}

/*
    length [Max Message]
        Set the display length in ms.
*/
void ll_mcwaveform_length(t_ll_mcwaveform *x, double f){
    x->ms_list.length = f;
    ll_mcwaveform_bang(x);
}

/*
    selstart [Max Message]
        Set the selection start time in ms.
*/
void ll_mcwaveform_selstart(t_ll_mcwaveform *x, double f){
    x->ms_list.sel_start = f;
    ll_mcwaveform_bang(x);
}

/*
    selend [Max Message]
        Set the selection end time in ms.
*/
void ll_mcwaveform_selend(t_ll_mcwaveform *x, double f){
    x->ms_list.sel_end = f;
    ll_mcwaveform_bang(x);
}

/*
    zoom2sel [Max Message]
        Set the display start & length equal to the selected start & end times.
*/
void ll_mcwaveform_zoom2sel(t_ll_mcwaveform *x){
    x->ms_list.start = x->ms_list.sel_start;
    x->ms_list.length = x->ms_list.sel_end - x->ms_list.sel_start;
    ll_mcwaveform_reread(x);
}

/*
    sel_disp [Max Message]
        Select the displayed portion of the waveform.
*/
void ll_mcwaveform_sel_disp(t_ll_mcwaveform *x){
    x->ms_list.sel_start = x->ms_list.start;
    x->ms_list.sel_end = x->ms_list.start + x->ms_list.length;
    ll_mcwaveform_bang(x);
}

/*
    sel_all [Max Message]
        Select the entire waveform.
*/
void ll_mcwaveform_sel_all(t_ll_mcwaveform *x){
    x->ms_list.start = 0;
    x->ms_list.length = x->l_length;

    x->ms_list.sel_start = 0;
    x->ms_list.sel_end = x->l_length;
    ll_mcwaveform_bang(x);
}

/*
    full [Max Message]
        Display full waveform (no change to selection).
*/
void ll_mcwaveform_full(t_ll_mcwaveform *x){
    x->ms_list.start = 0;
    x->ms_list.length = x->l_length;
    ll_mcwaveform_reread(x);
}


/**************************************************************************************************
    
    Paint Methods
        (reread, paint, paint_wf)

*************************************************************************************************/

/*
    reread
        Update object drawing, including the waveform
        In sf_mode, we set sf_read to re-read the audio file into the buffer on drawing
*/
void ll_mcwaveform_reread(t_ll_mcwaveform *x){
    x->wf_paint = 1;
    x->sf_read = 1;
    ll_mcwaveform_bang(x);
}

/*
    paint
        Called from bang function only via jbox_redraw().
        If wf_paint, redraw the waveform separately in paint_wf().
*/
void ll_mcwaveform_paint(t_ll_mcwaveform *x, t_object *view){
    t_rect rect;
    t_jgraphics *g;
    g = (t_jgraphics*) patcherview_get_jgraphics(view);
    jbox_get_rect_for_view((t_object *)x, view, &rect);

    // If display start or length has changed, redraw the waveform.
    if ((x->ms_list.start != x->msold[0]) || (x->ms_list.length != x->msold[1])) {
        x->msold[0] = x->ms_list.start;
        x->msold[1] = x->ms_list.length;
        x->wf_paint = 1;
    }

    if(x->chans != x->chans_old || x->chan_offset != x->chan_offset_old) {
        x->wf_paint = 1;
        x->chans_old = x->chans;
        x->chan_offset_old = 0;
    }

    if (x->wf_paint)
        ll_mcwaveform_paint_wf(x, view, &rect);

    jbox_paint_layer((t_object *)x, view, gensym("wf"), 0., 0.);
    jgraphics_set_source_jrgba(g, &x->ll_selcolor);

    // Draw selected portion over waveform.
    if (x->inv_sel_color) {
     jgraphics_rectangle_fill_fast(g,0, 0 ,(x->ms_list.sel_start-x->ms_list.start)/x->ms_list.length*rect.width, rect.height);
     jgraphics_rectangle_fill_fast(g,(x->ms_list.sel_end-x->ms_list.start)/x->ms_list.length*rect.width, 0 ,rect.width-(x->ms_list.sel_end-x->ms_list.start)/x->ms_list.length*rect.width, rect.height);
    }
    else{
     jgraphics_rectangle_fill_fast(g,(x->ms_list.sel_start-x->ms_list.start)/x->ms_list.length*rect.width, 0 ,(x->ms_list.sel_end-x->ms_list.sel_start)/x->ms_list.length*rect.width, rect.height);
    }

    // Draw line.
    if(x->linepos >= 0){
        double line_position = (x->linepos - x->ms_list.start) / x->ms_list.length * rect.width;
        jgraphics_set_source_jrgba(g, &x->ll_linecolor);
        jgraphics_move_to(g, line_position, 0);
        jgraphics_line_to(g, line_position, rect.height);
        jgraphics_set_line_width(g, 1);
    }
    jgraphics_stroke(g);
}

/*
    paint_wf
        Read from a buffer~ object and draw the waveform.
        In sf_mode, we read and write from an internal buffer when necessary.
        Otherwise, we read directly from the buffer referenced by the user.
*/
void ll_mcwaveform_paint_wf(t_ll_mcwaveform *x, t_object *view, t_rect *rect) {
    long i, j, k, co;
    float stepsize, ro, dispstart_ms, maxf, minf, samplef, v_zeropos, line_len;
    short peek_amt;

    double vzoom = x->vzoom_adj;

    long chns = x->chans == 0 ? x->l_chan : x->chans;
    long chn_height = rect->height / chns;

    t_float *tab;
    t_buffer_obj *buffer = buffer_ref_getobject(x->l_buffer_reference);

    jbox_invalidate_layer((t_object *)x, NULL, gensym("wf"));
    t_jgraphics *g = jbox_start_layer((t_object *)x, view, gensym("wf"), rect->width, rect->height);

    if(!g){
        object_error((t_object *)x, "t_jgraphics is NULL.");
        x->wf_paint = 0;
        return;
    }

    jgraphics_set_source_jrgba(g, &x->ll_bgcolor);
    jgraphics_rectangle_fill_fast(g, 0 , 0, rect->width, rect->height);
    jgraphics_set_source_jrgba(g, &x->ll_wfcolor);

    if (x->sf_mode){
        if (x->sf_read){ // missing samples need to be read from soundfile
            stepsize = x->ms_list.length / rect->width; //stepsize in ms
            dispstart_ms = x->ms_list.start;
            peek_amt = fmax(1, fmin(stepsize * x->l_srms, 600));
            //srms (samples per millisecond)
            //peek_amt is in samples! minimum 1, maximum 600
            x->l_arr_start = x->ms_list.start;  //??
            x->l_arr_len = x->ms_list.length;
            //post("peakamount %d stepsize %f width %f", peek_amt,stepsize, rect->width);

            for (i = 0; i < rect->width; i++){ //pixelwise in x-direction
                atom_setfloat(x->msg + 1, i * stepsize + dispstart_ms); //start reading at ms
                atom_setfloat(x->msg + 2, peek_amt/x->l_srms); //duration
                object_method_typed(x->buffer, gensym("read"), 4, x->msg, &x->rv);
                //post("read %d %f %f %d", i, i * stepsize + dispstart_ms, peek_amt/x->l_srms, peek_amt);
                tab = buffer_locksamples(buffer); //lock buffer and get pointer
                if(!tab){
                    // post("buffer disappeared while trying to read!");
                    x->wf_paint = 0;
                    return;
                }
                for (k=0; k<chns; k++){ // vertical-chans
                    maxf=0;
                    minf=0;
                    for (j=0;j<peek_amt;j++){
                        samplef = tab[x->l_chan * j + k + x->chan_offset]; //peek the buffer
                        maxf = fmax(samplef,maxf) / vzoom; //find max
                        minf = fmin(samplef,minf) / vzoom; //find min
                        //if (i<4) post("sampl %f indx %d", samplef, x->l_chan * j + k + x->chan_offset);
                    }

                    if (minf*-1 > maxf)
                        maxf = minf;
                    //post("maxf %f", maxf);
                    x->buf_arr[i][k] = maxf; //############## write into array
                    v_zeropos = (chn_height / 2 + k * chn_height);
                    line_len = maxf * chn_height / 2;

                    if (peek_amt > 10){ //draw up and down
                        jgraphics_move_to(g, i, v_zeropos + line_len);
                        jgraphics_line_to(g, i, v_zeropos - line_len);
                    }
                    else {  //draw only in direction of fmax
                        jgraphics_move_to(g, i, v_zeropos);
                        jgraphics_line_to(g, i, v_zeropos - line_len);
                    }
                }
                buffer_unlocksamples(buffer);
            }
            x->sf_read = 0;
        }
        else{  //draw from previously stored array
            long xarr;
            stepsize = x->ms_list.length / x->l_arr_len; //stepsize in array r
            dispstart_ms = rect->width * (x->ms_list.start - x->l_arr_start) / x->l_arr_len;
            peek_amt = fmax(1, fmin(x->l_srms * x->ms_list.length / rect->width, 600));
            
            for (i=0; i<rect->width; i++){
                for (k=0; k<chns; k++){ //k<x->l_chan
                    xarr = (int)round(i * stepsize + dispstart_ms);

                    if (xarr >= 0 && xarr <= rect->width)
                        maxf = x->buf_arr[xarr][k + x->chan_offset];
                    else
                        maxf = 0;

                    v_zeropos = (chn_height / 2) + (k * chn_height);
                    line_len = (maxf / vzoom) * (chn_height / 2);

                    if (peek_amt > 10){ //draw up and down
                        jgraphics_move_to(g, i, v_zeropos + line_len);
                        jgraphics_line_to(g, i, v_zeropos - line_len);
                    }
                    else {  //draw only in direction of fmax
                        jgraphics_move_to(g, i, v_zeropos);
                        jgraphics_line_to(g, i, v_zeropos - line_len);
                    }
                }
            }
        }
    } else if(buffer_ref_exists(x->l_buffer_reference)){ //in set-mode peek from buffer
        tab = buffer_locksamples(buffer);
        if(!tab){
            x->wf_paint = 0;
            return;
        }
        stepsize = x->ms_list.length * x->l_srms / rect->width; //stepsize in frames
        ro = x->ms_list.start * x->l_srms; //dispstart in samples
        peek_amt = fmax(1, fmin(stepsize, 600));
        //post("peakamount %d stepsize %f", peek_amt,stepsize);
        for (i=0; i<rect->width; i++){
            for (k=0; k<chns; k++){
                maxf = 0;
                minf = 0;
    
                for (j=0; j<peek_amt; j++){
                    samplef = tab[x->l_chan * ((int)roundl(i * stepsize + ro) + j) + k + x->chan_offset];
                    maxf = fmax(samplef, maxf) / vzoom;
                    minf = fmin(samplef, minf) / vzoom;
                }
    
                if ((minf * -1) > maxf)
                    maxf = minf;
    
                v_zeropos = chn_height / 2 + k * chn_height;
                line_len = maxf * chn_height / 2;
                if (peek_amt > 10){ //draw up and down
                    jgraphics_move_to(g, i, v_zeropos + line_len);
                    jgraphics_line_to(g, i, v_zeropos - line_len);
                }
                else {  //draw only in direction of fmax
                    jgraphics_move_to(g, i, v_zeropos);
                    jgraphics_line_to(g, i, v_zeropos - line_len);
                }
            }
        }

        buffer_unlocksamples(buffer);
    }
    jgraphics_set_line_width(g, 1);
    jgraphics_stroke(g);
    jbox_end_layer((t_object *)x, view, gensym("wf"));
    x->wf_paint = 0;
}


/**************************************************************************************************
    
    Mouse & Key Methods
        (mouseenter, mousedown, mousemove, mouseup, mousedrag)

*************************************************************************************************/

/*
    setmousecursor
        Set cursor appearance.
*/
void ll_mcwaveform_setmousecursor(t_ll_mcwaveform *x, t_object *patcherview, t_pt pt, long modifiers){
    t_rect rect;
    jbox_get_rect_for_view((t_object *)x, patcherview, &rect);
    t_jmouse_cursortype cursorType = JMOUSE_CURSOR_ARROW; // Default cursor type
    if(x->mouse_over){
        switch (x->mouse_mode) {
            case MOUSE_MODE_MOVE:
                cursorType = JMOUSE_CURSOR_DRAGGINGHAND;
                break;
            case MOUSE_MODE_SELECT:
                cursorType = JMOUSE_CURSOR_IBEAM;
                break;
            case MOUSE_MODE_LOOP:
                cursorType = JMOUSE_CURSOR_RESIZE_FOURWAY;
                break;
            case MOUSE_MODE_DRAW:
                cursorType = JMOUSE_CURSOR_CROSSHAIR;
                break;
        }
    }
    // Set the cursor once at the end of the function
    jmouse_setcursor(patcherview, (t_object *)x, cursorType);
}

/*
    mouseenter
        When the cursor enters the object's viewbox, change the mouse cursor appearance.
*/
void ll_mcwaveform_mouseenter(t_ll_mcwaveform *x, t_object *patcherview, t_pt pt, long modifiers){
    x->mouse_over = 1;
    ll_mcwaveform_setmousecursor(x, patcherview, pt, modifiers);
}

/*
    mouseleave
        When the cursor leaves the object's viewbox, change the mouse cursor appearance.
*/
void ll_mcwaveform_mouseleave(t_ll_mcwaveform *x, t_object *patcherview, t_pt pt, long modifiers){
    x->mouse_over = 0;
    ll_mcwaveform_setmousecursor(x, patcherview, pt, modifiers);
}

/*
    mousemove
        When the cursor is hovering over the object, change the mouse cursor appearance.
*/
void ll_mcwaveform_mousemove(t_ll_mcwaveform *x, t_object *patcherview, t_pt pt, long modifiers) {
    ll_mcwaveform_setmousecursor(x,patcherview,pt,modifiers);
}

/*
    mousedown
        Perform change to selection/display based on current mouse mode.
*/
void ll_mcwaveform_mousedown(t_ll_mcwaveform *x, t_object *patcherview, t_pt pt, long modifiers) {
    x->mouse_down = 1;
    s_ll_mcwaveform_cum = pt;
    
    t_rect rect;
    jbox_get_rect_for_view((t_object *)x, patcherview, &rect);
    
    // Calculate whether the Shift key was held down (using bitwise operation for clarity).
    char shift = (modifiers >> 1) & 1;
    
    switch (x->mouse_mode) {
        case MOUSE_MODE_DRAW: {
            // In "draw" mode, calculate and output mouse position within waveform.
            t_atom cx;
            double relativeX = pt.x * x->ms_list.length / rect.width + x->ms_list.start;
            atom_setfloat(&cx, relativeX);
            outlet_anything(x->ll_box.b_ob.o_outlet, gensym("mspos"), 1, &cx);
            break;
        }
        case MOUSE_MODE_SELECT: {
            // In "select" mode, adjust selection start and end based on mouse position and modifiers.
            s_ll_ccum = pt;
            double cx = pt.x * x->ms_list.length / rect.width + x->ms_list.start;
            
            if (!shift) {
                x->ms_list.sel_start = cx;
                x->ms_list.sel_end = cx;
            } else {
                // Adjust selection based on proximity to start or end.
                if (fabs(cx - x->ms_list.sel_start) < fabs(cx - x->ms_list.sel_end)){
                    s_ll_ccum.x = (x->ms_list.sel_end - x->ms_list.start) / x->ms_list.length * rect.width;
                } else {
                    s_ll_ccum.x = (x->ms_list.sel_start - x->ms_list.start) / x->ms_list.length * rect.width;
                }
                if (cx < x->ms_list.sel_start)
                    x->ms_list.sel_start = cx;
                else
                    x->ms_list.sel_end = cx;
            }
            break;
        }
        case MOUSE_MODE_LOOP: {
            double cx = pt.x * x->ms_list.length / rect.width + x->ms_list.start;

            double selected_len = x->ms_list.sel_end - x->ms_list.sel_start;
            double selected_half = selected_len / 2;

            x->ms_list.sel_start = cx - selected_half;
            x->ms_list.sel_end = cx + selected_half;
        }
        default:
            // Handle other modes, if necessary, or do nothing.
            break;
    }
}

/*
    mouseup
        When the mouse button is released, reread if necessary.
*/
void ll_mcwaveform_mouseup(t_ll_mcwaveform *x, t_object *patcherview, t_pt pt, long modifiers){
    x->mouse_down = 0;
    if (x->sf_mode && (x->mouse_mode == MOUSE_MODE_MOVE))
        ll_mcwaveform_reread(x);

    if(x->mouse_mode == MOUSE_MODE_LOOP || x->mouse_mode == MOUSE_MODE_SELECT)
        ll_mcwaveform_reread(x);
}

/*
    mousedrag:
        On mouse drag, adjust selection and displayed amount of waveform.
*/
void ll_mcwaveform_mousedrag(t_ll_mcwaveform *x, t_object *patcherview, t_pt pt, long modifiers) {
    char shift = modifiers / 2 % 2;
    s_ll_delta.x = pt.x - s_ll_mcwaveform_cum.x;
    s_ll_delta.y = shift ? 0 : pt.y - s_ll_mcwaveform_cum.y;

    s_ll_mcwaveform_cum = pt; // Update the cumulative mouse position
    t_rect rect;
    jbox_get_rect_for_view((t_object *)x, patcherview, &rect);

    double scaleFactor = x->ms_list.length / rect.width;
    double newXPosition = pt.x * scaleFactor + x->ms_list.start;
    double deltaYXScale = (2 * s_ll_delta.y + s_ll_delta.x) * scaleFactor;
    switch (x->mouse_mode) {
        case MOUSE_MODE_SELECT: {
            // Calculate the start and end points based on the current and previous cursor positions
            if (pt.x < s_ll_ccum.x) {
                x->ms_list.sel_start = newXPosition;
                x->ms_list.sel_end = s_ll_ccum.x * scaleFactor + x->ms_list.start;
            } else {
                x->ms_list.sel_end = newXPosition;
                x->ms_list.sel_start = s_ll_ccum.x * scaleFactor + x->ms_list.start;
            }
            break;
        }
        case MOUSE_MODE_LOOP: {
            // Adjust loop points based on mouse movement, ensuring loop end is always after loop start
            double adjustedStart = fmin(x->ms_list.sel_end - 0.0001, x->ms_list.sel_start + deltaYXScale);
            double adjustedEnd = fmax(x->ms_list.sel_start + 0.0001, x->ms_list.sel_end + (-2 * s_ll_delta.y + s_ll_delta.x) * scaleFactor);
            x->ms_list.sel_start = adjustedStart;
            x->ms_list.sel_end = adjustedEnd;
            break;
        }
        case MOUSE_MODE_MOVE: {
            // Move both the start and end of the selection
            double moveYScale = 4 * s_ll_delta.y * scaleFactor;
            x->ms_list.length += moveYScale;
            x->ms_list.start -= deltaYXScale;
            break;
        }
        case MOUSE_MODE_DRAW: {
            t_atom cx;
            atom_setfloat(&cx, newXPosition); // Note: This sets a float; if you need double precision, consider an alternative method for output
            outlet_anything(x->ll_box.b_ob.o_outlet, gensym("mspos"), 1, &cx);
            return; // Early return for draw mode
        }
        default:
            break; // No action for default case
    }
    ll_mcwaveform_bang(x); // Update UI or state as necessary
}

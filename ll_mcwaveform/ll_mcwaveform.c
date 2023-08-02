/*
 ll_mcwaveform
 by klaus filip 2020
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

static t_class	*s_ll_mcwaveform_class = 0;
static t_pt s_ll_mcwaveform_cum; // mouse tracking
static t_pt s_ll_ccum;
static t_pt s_ll_delta;

typedef struct _ll_mcwaveform
{
    t_jbox		ll_box;
    double      ll_width;
    double      ll_height;
    
    char        ll_mouseover;
    char        inv_sel_color;
    short       vmodeI;
    char        vmode;
    short       sf_mode;
    short       sf_read;
    short       wf_paint;
    short       buf_found;
    short       m_down;
    long       chans;
    long       chan_offset;

    

    long        ll_rectsize;
    long        mouse_shape;


    t_jrgba		ll_wfcolor;
    t_jrgba		ll_selcolor;
    t_jrgba		ll_bgcolor;
    t_jrgba		ll_linecolor;



    t_buffer_ref *l_buffer_reference;
    t_symbol    *bufname;
    long        l_chan;
    double      l_srms;
    double      l_length;
    double      l_arr_start;
    double      l_arr_len;
    unsigned long int l_frames;
    t_float		buf_arr[10000][30];
    double      mslist[4];
    double      msold[2];
    double      linepos;
    double      vzoom;

    
    t_object *buffer;
    t_atom *path;
    t_atom msg[4], rv;
} t_ll_mcwaveform;
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ basic
void *ll_mcwaveform_new(t_symbol *s, short argc, t_atom *argv);
void ll_mcwaveform_assist(t_ll_mcwaveform *x, void *b, long m, long a, char *s);
void ll_mcwaveform_free(t_ll_mcwaveform *x);
//t_max_err ll_mcwaveform_notify(t_ll_mcwaveform *x, t_symbol *s, t_symbol *msg, void *sender, void *data);
void myobject_iterator(t_ll_mcwaveform *x, t_object *b);
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ buffer
void ll_mcwaveform_set(t_ll_mcwaveform *x, t_symbol *s);
void ll_mcwaveform_sf(t_ll_mcwaveform *x, t_symbol *s, long ac, t_atom *av);
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ input types
void ll_mcwaveform_bang(t_ll_mcwaveform *x);
void ll_mcwaveform_int(t_ll_mcwaveform *x, long n);
void ll_mcwaveform_float(t_ll_mcwaveform *x, double f);
void ll_mcwaveform_list(t_ll_mcwaveform *x, t_symbol *s, short ac, t_atom *av);
void ll_mcwaveform_mode(t_ll_mcwaveform *x, t_symbol *s, long ac, t_atom *av);
void ll_mcwaveform_chans(t_ll_mcwaveform *x, t_symbol *s, long ac, t_atom *av);
void ll_mcwaveform_vzoom(t_ll_mcwaveform *x, double f);
void ll_mcwaveform_line(t_ll_mcwaveform *x, double f);
void ll_mcwaveform_start(t_ll_mcwaveform *x, double f);
void ll_mcwaveform_length(t_ll_mcwaveform *x, double f);
void ll_mcwaveform_selstart(t_ll_mcwaveform *x, double f);
void ll_mcwaveform_selend(t_ll_mcwaveform *x, double f);
void ll_mcwaveform_full(t_ll_mcwaveform *x);
void ll_mcwaveform_zoom2sel(t_ll_mcwaveform *x);
void ll_mcwaveform_sel_all(t_ll_mcwaveform *x);
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ paint
void ll_mcwaveform_reread(t_ll_mcwaveform *x);
void ll_mcwaveform_paint(t_ll_mcwaveform *x, t_object *view);
void ll_mcwaveform_paint_wf(t_ll_mcwaveform *x, t_object *view, t_rect *rect);
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ mouse &key
void ll_mcwaveform_mouseenter(t_ll_mcwaveform *x, t_object *patcherview, t_pt pt, long modifiers);
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
                  0L);
    
    c->c_flags |= CLASS_FLAG_NEWDICTIONARY; // to specify dictionary constructor
    
    jbox_initclass(c, JBOX_COLOR );
    //jbox_initclass(c, JBOX_TEXTFIELD | JBOX_FIXWIDTH | JBOX_COLOR | JBOX_FONTATTR);
    
    class_addmethod(c, (method)ll_mcwaveform_paint,			"paint", A_CANT, 0);
    class_addmethod(c, (method)ll_mcwaveform_int,           "int", A_LONG, 0);
    class_addmethod(c, (method)ll_mcwaveform_float,			"float", A_FLOAT, 0);
    class_addmethod(c, (method)ll_mcwaveform_list,			"list", A_GIMME, 0);
    class_addmethod(c, (method)ll_mcwaveform_bang,			"bang", 0);
    class_addmethod(c, (method)ll_mcwaveform_mode,			"mode",		A_GIMME, 0);
    class_addmethod(c, (method)ll_mcwaveform_chans,			"chans",		A_GIMME, 0);
    class_addmethod(c, (method)ll_mcwaveform_line,          "line", A_FLOAT, 0);
    class_addmethod(c, (method)ll_mcwaveform_vzoom,          "vzoom", A_FLOAT, 0);
    class_addmethod(c, (method)ll_mcwaveform_start,         "start", A_FLOAT, 0);
    class_addmethod(c, (method)ll_mcwaveform_length,         "length", A_FLOAT, 0);
    class_addmethod(c, (method)ll_mcwaveform_selstart,       "selstart", A_FLOAT, 0);
    class_addmethod(c, (method)ll_mcwaveform_selend,         "selend", A_FLOAT, 0);
    class_addmethod(c, (method)ll_mcwaveform_zoom2sel,			"zoom2sel", 0);
    class_addmethod(c, (method)ll_mcwaveform_sel_all,			"sel_all", 0);
    class_addmethod(c, (method)ll_mcwaveform_full,			"full", 0);
    class_addmethod(c, (method)ll_mcwaveform_reread,			"reread", 0);


    class_addmethod(c, (method) ll_mcwaveform_mousemove,	"mousemove", A_CANT, 0);
    class_addmethod(c, (method)ll_mcwaveform_mousedown,		"mousedown", A_CANT, 0);
    class_addmethod(c, (method)ll_mcwaveform_mousedrag,     "mousedrag", A_CANT, 0);
    class_addmethod(c, (method)ll_mcwaveform_mouseup,       "mouseup", A_CANT, 0);
    class_addmethod(c, (method)ll_mcwaveform_mouseenter,    "mouseenter",   A_CANT, 0);

    class_addmethod(c, (method)ll_mcwaveform_assist,          "assist", A_CANT, 0);
 
    class_addmethod(c, (method)ll_mcwaveform_set,           "set", A_SYM, 0);
    class_addmethod(c, (method)ll_mcwaveform_sf,            "sf", A_GIMME, 0);
    
    CLASS_ATTR_DEFAULT(c,"patching_rect",0, "0. 0. 200. 100.");

    
    //**********fonts
    //CLASS_STICKY_ATTR(c,"category",0,"Font");
    //CLASS_ATTR_DEFAULTNAME_SAVE_PAINT(c,"fontname",0,"Arial");
    //CLASS_ATTR_DEFAULTNAME_SAVE_PAINT(c,"fontsize",0,"12");
    //**********colors
    CLASS_STICKY_ATTR(c, "category", 0, "Color");

    CLASS_ATTR_RGBA_LEGACY(c,			"wfcolor","frgb", 0, t_ll_mcwaveform, ll_wfcolor);
    CLASS_ATTR_DEFAULTNAME_SAVE_PAINT(c,"wfcolor",0,"0.55 0.2 0.84 1.");
    CLASS_ATTR_STYLE_LABEL(c,			"wfcolor",0,"rgba","Waveform Color");
    
    CLASS_ATTR_RGBA_LEGACY(c,			"selcolor", "rgb2",0, t_ll_mcwaveform, ll_selcolor);
    CLASS_ATTR_DEFAULT_SAVE_PAINT(c,	"selcolor",0,"0.24 0.5 0.8 0.55");
    CLASS_ATTR_STYLE_LABEL(c,			"selcolor",0,"rgba","Select Color");
    
    CLASS_ATTR_RGBA_LEGACY(c,			"bgcolor", "brgb", 0, t_ll_mcwaveform, ll_bgcolor);
    CLASS_ATTR_DEFAULTNAME_SAVE_PAINT(c,"bgcolor",0,"0.8 0.8 0.8 1.");
    CLASS_ATTR_STYLE_LABEL(c,			"bgcolor", 0, "rgba", "Background Color");
    
    CLASS_ATTR_RGBA_LEGACY(c,			"linecolor","rgb3", 0, t_ll_mcwaveform, ll_linecolor);
    CLASS_ATTR_DEFAULTNAME_SAVE_PAINT(c,"linecolor",0,"0. 0. 0. 1.");
    CLASS_ATTR_STYLE_LABEL(c,			"linecolor",0,"rgba","Line Color");
    
    CLASS_ATTR_CHAR(c,				"inv_sel_color", 0, t_ll_mcwaveform, inv_sel_color);
    CLASS_ATTR_STYLE(c,             "inv_sel_color", 0, "onoff");
    CLASS_ATTR_STYLE_LABEL(c,		"inv_sel_color", 0, "onoff", "Invert Selection Color");
    CLASS_ATTR_DEFAULT_SAVE_PAINT(c,"inv_sel_color",0,"0");
    
    CLASS_ATTR_CHAR(c,				"setmode", 0, t_ll_mcwaveform, vmode);
    CLASS_ATTR_STYLE_LABEL(c,		"setmode", 0, "enum", "click mode");
    CLASS_ATTR_ENUMINDEX(c,			"setmode", 0, "none select loop move");
    CLASS_ATTR_DEFAULT_SAVE_PAINT(c,"setmode",0,"1");

    CLASS_STICKY_ATTR_CLEAR(c, "category");
    CLASS_ATTR_ORDER(c, "wfcolor",			0, "1");
    CLASS_ATTR_ORDER(c, "selcolor",		0, "2");
    CLASS_ATTR_ORDER(c, "bgcolor",		0, "3");
    CLASS_ATTR_ORDER(c, "linecolor",      0, "4");

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
    | JBOX_DRAWINLAST			// rbs -- I think we can turn this off
    //		| JBOX_TRANSPARENT
    //		| JBOX_NOGROW
    //		| JBOX_GROWY
    | JBOX_GROWBOTH
    | JBOX_HILITE
    | JBOX_DRAWBACKGROUND
    //      | JBOX_MOUSEDRAGDELTA
    //		| JBOX_NOFLOATINSPECTOR
    //		| JBOX_TEXTFIELD
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
    //post("w %lf h %lf", x->ll_width, x->ll_height );
    x->msold[0]=-1;
    x->msold[1]=-1;
    x->chans = 0;
    x->chan_offset = 0;
    x->sf_mode = -1;
    x->vzoom = 1.0;
    //ll_mcwaveform_bang(x);
    
    return x;
}
void ll_mcwaveform_assist(t_ll_mcwaveform *x, void *b, long m, long a, char *s){
    if (m==ASSIST_INLET) {
        sprintf(s,"list");
    }
    else {
        sprintf(s,"list: start, length, selstart, selend");
    }
}
void ll_mcwaveform_free(t_ll_mcwaveform *x){
    object_free(x->l_buffer_reference);
    jbox_free(&x->ll_box);
}

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ buffer
void ll_mcwaveform_set(t_ll_mcwaveform *x, t_symbol *s){
    t_float		*tab;
    x->sf_mode = 0;
    if (!x->l_buffer_reference)
        x->l_buffer_reference = buffer_ref_new((t_object *)x, s);
    else
        buffer_ref_set(x->l_buffer_reference, s);
    t_buffer_obj	*buffer = buffer_ref_getobject(x->l_buffer_reference);
    tab = buffer_locksamples(buffer);
    x->l_chan = buffer_getchannelcount(buffer);
    x->l_frames = buffer_getframecount(buffer);
    x->l_srms = buffer_getmillisamplerate(buffer);
    buffer_unlocksamples(buffer);
    x->l_length = x->l_frames / x->l_srms;
    x->mslist[0] = 0;
    x->mslist[1] = x->l_length;
    //post("frames %d channels %d srms %lf length %lf",x->l_frames, x->l_chan, x->l_srms, x->l_length);
    
    x->wf_paint=1;
    ll_mcwaveform_bang(x);
}
void myobject_iterator(t_ll_mcwaveform *x, t_object *b){
    t_object *box, *obj, *jkl;
    t_symbol *name;
    t_rect rect;
    char str[13];
    x->buf_found = 0;
    for (box = jpatcher_get_firstobject(b); box; box = jbox_get_nextobject(box)) { //look for an buffer~ with scripting name wfbuf
        obj = jbox_get_object(box);
        if (obj){
            //post("f %s",object_classname(obj)->s_name);
            if (strcmp(object_classname(obj)->s_name,"ll_mcwaveform")==0){ //also tell the rect of the wf-box
                object_attr_get_rect(box, gensym("patching_rect"),&rect);
                //post("re %f %f",rect.x,rect.y);
            }
            if (strcmp(object_classname(obj)->s_name,"buffer~")==0){
                name = object_attr_getsym(box, gensym("varname"));
                if (name==gensym("mcwfbuf")){
                    //post("found class %s mcwfbuf",object_classname(obj)->s_name);
                    x->buffer = obj;
                    x->buf_found = 1;
                }
            }
        }
    }
    
    if(x->buf_found==0){ //script buffer~ and scriptname it
        //post("buffer not found");
        jkl = newobject_sprintf(b, "@maxclass newobj @text \"buffer~ mcwfbuf\" @varname mcwfbuf @patching_position %.2f %.2f",rect.x,rect.y);
        x->buffer = jbox_get_object(jkl);
        x->buf_found = 1;
    }
    sprintf(str, "mcwfbuf%d", rand()%100000); //give a unique name to the buffer
    x->bufname = gensym(str);
    atom_setsym(x->msg, gensym(str));
    object_method_typed(x->buffer, gensym("name"), 1, x->msg, &x->rv);
} //find or create the buffer~ object
void ll_mcwaveform_sf(t_ll_mcwaveform *x, t_symbol *s, long ac, t_atom *av){
    long i;
    t_atom *ap;
    x->sf_mode = 1;
    for (i = 0, ap = av; i < ac; i++, ap++){
        switch (atom_gettype(ap)) {
            case A_LONG:
                if (i==1) x->l_chan=atom_getlong(ap);
                if (i==2) x->l_srms=atom_getlong(ap)*0.001;
                //post("%ld: %ld",i+1,atom_getlong(ap));
                break;
            case A_FLOAT:
                if (i==3) x->l_length=atom_getfloat(ap);
                //post("%ld: %.2f",i+1,atom_getfloat(ap));
                break;
            case A_SYM:
                if (i==0) x->path = ap;
                //post("%ld: %s %s",i+1,atom_getsym(ap)->s_name,atom_getsym(path)->s_name);
                //msg[0] = *ap;
                
                break;
            default:
                //post("%ld: unknown atom type (%ld)", i+1, atom_gettype(ap));
                break;
        }
    } //process arguments
    //post("path %s ch %d srms %f len %f",atom_getsym(x->path)->s_name,x->l_chan,x->l_srms,x->l_length);
    x->mslist[0]=0;
    x->mslist[1]=x->l_length;
    t_object *patcher;
    t_max_err err;
    err = object_obex_lookup(x, gensym("#P"), &patcher);
    myobject_iterator(x, patcher); //get buffer object
    if (x->buf_found) {
        atom_setlong(x->msg, 600);
        object_method_typed(x->buffer, gensym("sizeinsamps"), 1, x->msg, &x->rv);
        x->msg[0]=*x->path;
        atom_setlong(x->msg + 1, 0);
        atom_setlong(x->msg + 2, 600);
        atom_setlong(x->msg + 3, x->l_chan);
        object_method_typed(x->buffer, gensym("read"), 4, x->msg, &x->rv);
    
    if (!x->l_buffer_reference)
        x->l_buffer_reference = buffer_ref_new((t_object *)x, x->bufname);
    else
        buffer_ref_set(x->l_buffer_reference, x->bufname);
    x->sf_read = 1;
    x->wf_paint=1;
    ll_mcwaveform_bang(x);
    }
} //read from disc (first time)
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ input types
void ll_mcwaveform_bang(t_ll_mcwaveform *x){
    if (x->mslist[0]<0) x->mslist[0]=0;
    if (x->mslist[0]>x->l_length) x->mslist[0] = x->l_length-0.5;
    if (x->mslist[0]+x->mslist[1]>x->l_length) x->mslist[1]=x->l_length-x->mslist[0];
    if (x->mslist[1]<20/x->l_srms) x->mslist[1] = 20/x->l_srms;
    x->mslist[2] = fmax(0,x->mslist[2]);
    x->mslist[3] = fmin(x->l_length,x->mslist[3]);
    if (x->sf_mode>-1) jbox_redraw(&x->ll_box);
    t_atom myList[4];
    atom_setdouble_array(4,myList,4,x->mslist);
    outlet_anything(x->ll_box.b_ob.o_outlet, gensym("mslist"), 4,myList );
    //outlet_list(x->ll_box.b_ob.o_outlet,0L,4,myList);
}
void ll_mcwaveform_int(t_ll_mcwaveform *x, long n){
    //post("int");
    ll_mcwaveform_float(x, (double)n);
}
void ll_mcwaveform_float(t_ll_mcwaveform *x, double f){
    //post("float");
}
void ll_mcwaveform_list(t_ll_mcwaveform *x, t_symbol *s, short ac, t_atom *av){
    //post("list");
    //double hmm[4];
    if (ac<=4 && av) {
        //atom_getdouble_array(ac,av,4,hmm);
        //if (hmm[0]!=x->mslist[0] || hmm[1]!=x->mslist[1]) x->sf_read = 1;
        atom_getdouble_array(ac,av,4,x->mslist);
        //post("h %lf %lf %lf %lf", x->mslist[0],x->mslist[1],x->mslist[2],x->mslist[3]);
    }

    ll_mcwaveform_bang(x);
    
}
void ll_mcwaveform_mode(t_ll_mcwaveform *x, t_symbol *s, long ac, t_atom *av){
    if (ac && av) {
        if (atom_gettype(av) == A_SYM){
            //post("sym %s %d", atom_getsym(av)->s_name, strcmp(atom_getsym(av)->s_name, "move"));
            if (strcmp(atom_getsym(av)->s_name, "none")==0) x->vmode = 0;
            else if (strcmp(atom_getsym(av)->s_name, "select")==0) x->vmode = 1;
            else if (strcmp(atom_getsym(av)->s_name, "loop")==0) x->vmode = 2;
            else if (strcmp(atom_getsym(av)->s_name, "move")==0) x->vmode = 3;
            else if (strcmp(atom_getsym(av)->s_name, "draw")==0) x->vmode = 4;
            else object_error((t_object *)x, "bad argument for message mode");
            }
        else if (atom_gettype(av) == A_LONG){
            x->vmode = (int)atom_getlong(av);
        }
        else if (atom_gettype(av) == A_FLOAT)
            object_error((t_object *)x, "bad argument for message mode");

        else {
            object_error((t_object *)x, "bad argument for message mode");
            return;
        }
    }
}
void ll_mcwaveform_chans(t_ll_mcwaveform *x, t_symbol *s, long ac, t_atom *av){

    if (x->sf_mode>-1){
    if (ac==1 && av) {
        if (atom_gettype(av) == A_SYM){
            //post("sym %s %d", atom_getsym(av)->s_name, strcmp(atom_getsym(av)->s_name, "move"));
            if (strcmp(atom_getsym(av)->s_name, "all")==0) {
                x->chans = x->l_chan;
            }
            else {
                object_error((t_object *)x, "bad argument for message chans");
                return;
            }
        }
        else if (atom_gettype(av) == A_LONG){
            x->chans = atom_getlong(av);
        }
        else if (atom_gettype(av) == A_FLOAT){
            object_error((t_object *)x, "bad argument for message mode");
            return;
        }
    }
    
    else if (ac==2 && av) {
        if (atom_gettype(&av[0]) == A_LONG && atom_gettype(&av[1]) == A_LONG){
            x->chans = atom_getlong(&av[0]);
            x->chan_offset = atom_getlong(&av[1]);
        }
        else object_error((t_object *)x, "bad argument for message mode");
    }
    else {
        object_error((t_object *)x, "bad argument for message mode");
        return;
    }
        if(x->chans==0) x->chans = x->l_chan;
        x->chans = fmaxl(1,fminl(x->chans,x->l_chan));
        x->chan_offset = fmaxl(0,fminl(x->chan_offset,x->l_chan-x->chans));
    ll_mcwaveform_reread(x);
    }
    //post ("chans %d chan_offset %d sfm %d",x->chans,x->chan_offset,x->sf_mode);
    
}
void ll_mcwaveform_vzoom(t_ll_mcwaveform *x, double f){
    double new_zoom = 0.000001;
    if (f > 0.0) {
       new_zoom = pow(f, 0.003);
    } 
    x->vzoom=new_zoom;
    x->wf_paint=1;
    jbox_redraw(&x->ll_box);
    // ll_mcwaveform_reread(x);
}
void ll_mcwaveform_line(t_ll_mcwaveform *x, double f){
    x->linepos=f;
    jbox_redraw(&x->ll_box);
    //ll_mcwaveform_bang(x);
}
void ll_mcwaveform_start(t_ll_mcwaveform *x, double f){
    x->mslist[0]=f;
    ll_mcwaveform_bang(x);
}
void ll_mcwaveform_length(t_ll_mcwaveform *x, double f){
    x->mslist[1]=f;
    ll_mcwaveform_bang(x);
}
void ll_mcwaveform_selstart(t_ll_mcwaveform *x, double f){
    x->mslist[2]=f;
    ll_mcwaveform_bang(x);
}
void ll_mcwaveform_selend(t_ll_mcwaveform *x, double f){
    x->mslist[3]=f;
    ll_mcwaveform_bang(x);
}
void ll_mcwaveform_zoom2sel(t_ll_mcwaveform *x){
    x->mslist[0]=x->mslist[2];
    x->mslist[1]=x->mslist[3]-x->mslist[2];
    ll_mcwaveform_reread(x);
}
void ll_mcwaveform_sel_all(t_ll_mcwaveform *x){
    x->mslist[2]=x->mslist[0];
    x->mslist[3]=x->mslist[0]+x->mslist[1];
    ll_mcwaveform_bang(x);
}
void ll_mcwaveform_full(t_ll_mcwaveform *x){
    x->mslist[0]=0;
    x->mslist[1]=x->l_length;
    ll_mcwaveform_reread(x);
}
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ paint
void ll_mcwaveform_reread(t_ll_mcwaveform *x){
    x->wf_paint = 1;
    x->sf_read = 1;
    ll_mcwaveform_bang(x);
}
void ll_mcwaveform_paint(t_ll_mcwaveform *x, t_object *view){
    t_rect rect;
    t_jgraphics *g;
    g = (t_jgraphics*) patcherview_get_jgraphics(view);
    jbox_get_rect_for_view((t_object *)x, view, &rect);
    if (x->mslist[0]!=x->msold[0] || x->mslist[1]!=x->msold[1]) x->wf_paint=1;
    x->msold[0]=x->mslist[0];
    x->msold[1]=x->mslist[1];
    if (x->wf_paint) ll_mcwaveform_paint_wf(x, view, &rect);
    jbox_paint_layer((t_object *)x, view, gensym("wf"), 0., 0.);
    jgraphics_set_source_jrgba(g, &x->ll_selcolor);
    if (x->inv_sel_color) {
        jgraphics_rectangle_fill_fast(g,0, 0 ,(x->mslist[2]-x->mslist[0])/x->mslist[1]*rect.width, rect.height);
        jgraphics_rectangle_fill_fast(g,(x->mslist[3]-x->mslist[0])/x->mslist[1]*rect.width, 0 ,rect.width-(x->mslist[3]-x->mslist[0])/x->mslist[1]*rect.width, rect.height);
    }
    else{
        jgraphics_rectangle_fill_fast(g,(x->mslist[2]-x->mslist[0])/x->mslist[1]*rect.width, 0 ,(x->mslist[3]-x->mslist[2])/x->mslist[1]*rect.width, rect.height);
    }
    //post("r1 %f",(x->mslist[2]-x->mslist[0])/x->mslist[1]);
    jgraphics_set_source_jrgba(g, &x->ll_linecolor);
    jgraphics_move_to(g,(x->linepos-x->mslist[0])/x->mslist[1]*rect.width,0);
    jgraphics_line_to(g,(x->linepos-x->mslist[0])/x->mslist[1]*rect.width,rect.height);
    // post("linepos %f",(x->linepos-x->mslist[0])/x->mslist[1]*rect.width);
    jgraphics_set_line_width(g, 1);
    jgraphics_stroke(g);
}
void ll_mcwaveform_paint_wf(t_ll_mcwaveform *x, t_object *view, t_rect *rect){
    long i,j,k,co,chns;
    short peek_amt;
    float r,ro,maxf,minf,cf;
    
    double vzoom_amount = x->vzoom;

    t_float		*tab;
    t_buffer_obj	*buffer = buffer_ref_getobject(x->l_buffer_reference);
    jbox_invalidate_layer((t_object *)x, NULL, gensym("wf"));
    t_jgraphics *g = jbox_start_layer((t_object *)x, view, gensym("wf"), rect->width, rect->height);
    if (x->chans == 0) {chns = x->l_chan;}
    else chns = x->chans;
    //post("pa %d %f", peek_amt,r);
    //short tm;
    //tm = jkeyboard_getcurrentmodifiers();
    //post("mk %d",tm);
    if (g) {
        jgraphics_set_source_jrgba(g, &x->ll_bgcolor);
        jgraphics_rectangle_fill_fast(g, 0 , 0, rect->width, rect->height);
        jgraphics_set_source_jrgba(g, &x->ll_wfcolor);
        co = rect->height/chns;
        if (x->sf_mode == 1){
            if (x->sf_read){
                r = x->mslist[1]/rect->width; //stepsize in ms
                ro = x->mslist[0];
                peek_amt = fmax(1,fmin(r*x->l_srms,600));
                x->l_arr_start = x->mslist[0];
                x->l_arr_len = x->mslist[1];
                for (i=0; i<rect->width; i++){
                    atom_setlong(x->msg + 1, i*r+ro);
                    atom_setlong(x->msg + 2, peek_amt/x->l_srms);
                    object_method_typed(x->buffer, gensym("read"), 4, x->msg, &x->rv);
                    tab = buffer_locksamples(buffer);
                    for (k=0;k<chns;k++){ //k<x->l_chan
                        maxf=0;minf=0;
                        for (j=0;j<peek_amt;j++){
                            cf = tab[x->l_chan*j+k+x->chan_offset];
                            maxf = fmax(cf,maxf)/vzoom_amount;
                            minf = fmin(cf,minf)/vzoom_amount;
                        }
                        
                        if (minf*-1>maxf) maxf = minf;
                        x->buf_arr[i][k]= maxf;
                        //jgraphics_move_to(g,i,(co/2+k*co)-x->buf_arr[i][k]*co/2);
                        //jgraphics_line_to(g,i,(co/2+k*co)+x->buf_arr[i][k]*co/2);
                        if (peek_amt>200){
                            jgraphics_move_to(g,i,(co/2+k*co)-maxf*co/2);
                            jgraphics_line_to(g,i,(co/2+k*co)+maxf*co/2);
                        }
                        else {
                            jgraphics_move_to(g,i,(co/2+k*co));
                            jgraphics_line_to(g,i,(co/2+k*co)-maxf*co/2);
                        }
                        
                    }
                    buffer_unlocksamples(buffer);
                }
                x->sf_read = 0;
            }
            else{
                    long xarr;
                    r = x->mslist[1]/x->l_arr_len; //stepsize in array
                    ro = rect->width*(x->mslist[0]-x->l_arr_start)/x->l_arr_len;
                    peek_amt = fmax(1,fmin(x->l_srms*x->mslist[1]/rect->width,600));
                    
                    for (i=0; i<rect->width; i++){for (k=0;k<chns;k++){ //k<x->l_chan
                        xarr = (int)round(i*r+ro);
                        if (xarr>=0 && xarr<=rect->width) maxf = x->buf_arr[xarr][k+x->chan_offset]; else maxf = 0;
                        //post("i %d rr %f cf %f",i,i*r+ro,cf);


                        if (peek_amt>200){
                            jgraphics_move_to(g,i,(co/2+k*co)-maxf*co/2);
                            jgraphics_line_to(g,i,(co/2+k*co)+maxf*co/2);
                        }
                        else {
                            jgraphics_move_to(g,i,(co/2+k*co));
                            jgraphics_line_to(g,i,(co/2+k*co)-maxf*co/2);
                        }
                        
                    }}
                }
            
        }
        else if(buffer){
            tab = buffer_locksamples(buffer);
            r = x->mslist[1]*x->l_srms/rect->width; //stepsize in frames
            ro = x->mslist[0]*x->l_srms;
            peek_amt = fmax(1,fmin(r,600));
            for (i=0; i<rect->width; i++){for (k=0;k<chns;k++){ //k<x->l_chan
                    maxf=0;minf=0;
                    for (j=0;j<peek_amt;j++){
                        cf = tab[x->l_chan*((int)roundl(i*r+ro)+j)+k+x->chan_offset];
                        //if (cf<0) cf=-cf;
                        maxf = fmax(cf,maxf)/vzoom_amount;
                        minf = fmin(cf,minf)/vzoom_amount;
                    }
                    if (minf*-1>maxf) maxf = minf;
                //x->buf_arr[i][k]= maxf;
                //jgraphics_move_to(g,i,(co/2+k*co)-x->buf_arr[i][k]*co/2);
                //jgraphics_line_to(g,i,(co/2+k*co)+x->buf_arr[i][k]*co/2);
                    if (peek_amt>200){
                        jgraphics_move_to(g,i,(co/2+k*co)-maxf*co/2);
                        jgraphics_line_to(g,i,(co/2+k*co)+maxf*co/2);
                    }
                    else {
                        jgraphics_move_to(g,i,(co/2+k*co));
                        jgraphics_line_to(g,i,(co/2+k*co)-maxf*co/2);
                    }
            }}

            buffer_unlocksamples(buffer);
        }
        jgraphics_set_line_width(g, 1);
        jgraphics_stroke(g);
        jbox_end_layer((t_object *)x, view, gensym("wf"));
    }
    
    //jbox_paint_layer((t_object *)x, view, gensym("wf"), 0., 0.);	// position of the layer
    x->wf_paint = 0;
}
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ mouse &key
void ll_mcwaveform_mouseenter(t_ll_mcwaveform *x, t_object *patcherview, t_pt pt, long modifiers){
    //post("enter mod %ld", x->vmode);
    switch(x->vmode){
        case 3: //cmd
            jmouse_setcursor(patcherview, (t_object *) x, (t_jmouse_cursortype) JMOUSE_CURSOR_DRAGGINGHAND );
            break;
        case 1: //option
            jmouse_setcursor(patcherview, (t_object *) x, (t_jmouse_cursortype) JMOUSE_CURSOR_IBEAM  );
            break;
        case 2: //ctrl
            jmouse_setcursor(patcherview, (t_object *) x, (t_jmouse_cursortype) JMOUSE_CURSOR_RESIZE_FOURWAY );
            break;
        default:
             jmouse_setcursor(patcherview, (t_object *) x, (t_jmouse_cursortype) JMOUSE_CURSOR_ARROW );
            break;
    }
    //jmouse_setcursor(patcherview, (t_object *) x, (t_jmouse_cursortype) JMOUSE_CURSOR_IBEAM  );
    
   
}
void ll_mcwaveform_mousedown(t_ll_mcwaveform *x, t_object *patcherview, t_pt pt, long modifiers){
    x->m_down = 1;
    //post("md %d",x->m_down);
    s_ll_mcwaveform_cum = pt;
    t_rect rect;
    char shift;
    t_atom cx;
    shift = modifiers/2%2;
    jbox_get_rect_for_view((t_object *)x, patcherview, &rect);
    switch (x->vmode){
        case 4:
            atom_setfloat(&cx,pt.x*x->mslist[1]/rect.width+x->mslist[0]);
            outlet_anything(x->ll_box.b_ob.o_outlet, gensym("mspos"), 1,&cx );
            break;
        case 1:
            s_ll_ccum = pt;
            if (!shift) {
                x->mslist[2]=s_ll_mcwaveform_cum.x*x->mslist[1]/rect.width+x->mslist[0];
                x->mslist[3]=x->mslist[2];
            }
            else {
                double cx;
                cx =s_ll_mcwaveform_cum.x*x->mslist[1]/rect.width+x->mslist[0];
                if (fabs(cx-x->mslist[2])<fabs(cx-x->mslist[3])) s_ll_ccum.x = (x->mslist[3]-x->mslist[0])/x->mslist[1]*rect.width;
                else s_ll_ccum.x = (x->mslist[2]-x->mslist[0])/x->mslist[1]*rect.width;
            }
            //ll_mcwaveform_bang(x);
            break;
        default: break;
    }
}
void ll_mcwaveform_mousemove(t_ll_mcwaveform *x, t_object *patcherview, t_pt pt, long modifiers){
    //idle_mouse !!
    //post("mousemove_mod %d",modifiers);
    t_rect rect;
    jbox_get_rect_for_view((t_object *)x, patcherview, &rect);
    //pt.y = rect.height - pt.y;
    switch(x->vmode){
        case 3: //cmd
            //post("move mod %ld", x->vmode);
            jmouse_setcursor(patcherview, (t_object *) x, (t_jmouse_cursortype) JMOUSE_CURSOR_DRAGGINGHAND );
            break;
        case 1: //option
            jmouse_setcursor(patcherview, (t_object *) x, (t_jmouse_cursortype) JMOUSE_CURSOR_IBEAM  );
            break;
        case 2: //ctrl
            jmouse_setcursor(patcherview, (t_object *) x, (t_jmouse_cursortype) JMOUSE_CURSOR_RESIZE_FOURWAY );
            break;
        case 4: //ctrl
            jmouse_setcursor(patcherview, (t_object *) x, (t_jmouse_cursortype) JMOUSE_CURSOR_CROSSHAIR );
            break;
            
        default:
            jmouse_setcursor(patcherview, (t_object *) x, (t_jmouse_cursortype) JMOUSE_CURSOR_ARROW );
            break;
    }
    /*
    switch(modifiers){
        case 1: //cmd
            jmouse_setcursor(patcherview, (t_object *) x, (t_jmouse_cursortype) JMOUSE_CURSOR_DRAGGINGHAND ); break;
        case 3: //cmd-shift
            jmouse_setcursor(patcherview, (t_object *) x, (t_jmouse_cursortype) JMOUSE_CURSOR_DRAGGINGHAND ); break;
        case 8: //option
            jmouse_setcursor(patcherview,(t_object *)x,(t_jmouse_cursortype)JMOUSE_CURSOR_IBEAM);break;
        case 10: //option-shift
            jmouse_setcursor(patcherview,(t_object *)x,(t_jmouse_cursortype)JMOUSE_CURSOR_IBEAM);break;
        case 132: //ctrl
            jmouse_setcursor(patcherview,(t_object *)x,(t_jmouse_cursortype)JMOUSE_CURSOR_RESIZE_FOURWAY);break;
        case 134: //ctrl-shift
            jmouse_setcursor(patcherview,(t_object *)x,(t_jmouse_cursortype)JMOUSE_CURSOR_RESIZE_FOURWAY);break;
        default:
            jmouse_setcursor(patcherview, (t_object *) x, (t_jmouse_cursortype) JMOUSE_CURSOR_ARROW );
            break;
    }
     */

    
    
}
void ll_mcwaveform_mouseup(t_ll_mcwaveform *x, t_object *patcherview, t_pt pt, long modifiers){
    x->m_down = 0;
        if (x->sf_mode && x->vmode==3) {
            ll_mcwaveform_reread(x);
        }
    //post("mup %d", modifiers);
    //t_rect rect;
    //jbox_get_rect_for_view((t_object *)x, patcherview, &rect);
    
}
void ll_mcwaveform_mousedrag(t_ll_mcwaveform *x, t_object *patcherview, t_pt pt, long modifiers){
    char shift;
    shift = modifiers/2%2;
    s_ll_delta.x = pt.x-s_ll_mcwaveform_cum.x;
    if (!shift) s_ll_delta.y = pt.y-s_ll_mcwaveform_cum.y;
    else s_ll_delta.y =0;
    //post("dx %f dy %f", s_ll_delta.x, s_ll_delta.y);
    s_ll_mcwaveform_cum = pt;
    t_rect rect;
    t_atom cx;

    jbox_get_rect_for_view((t_object *)x, patcherview, &rect);
    
    switch (x->vmode){
        case 1: //select
            if (pt.x<s_ll_ccum.x){
                x->mslist[2]=pt.x*x->mslist[1]/rect.width+x->mslist[0];
                x->mslist[3]=s_ll_ccum.x*x->mslist[1]/rect.width+x->mslist[0];}
            else {
                x->mslist[3]=pt.x*x->mslist[1]/rect.width+x->mslist[0];
                x->mslist[2]=s_ll_ccum.x*x->mslist[1]/rect.width+x->mslist[0];}
            break;
        case 2: //loop
                x->mslist[2]=fmin(x->mslist[3]-0.0001,x->mslist[2]+(2*s_ll_delta.y+s_ll_delta.x)*x->mslist[1]/rect.width);
                x->mslist[3]=fmax(x->mslist[2]+0.0001,x->mslist[3]+(-2*s_ll_delta.y+s_ll_delta.x)*x->mslist[1]/rect.width);
            break;
        case 3: //move
            x->mslist[1]=x->mslist[1]+4*s_ll_delta.y*x->mslist[1]/rect.width;
            x->mslist[0]=x->mslist[0]-(2*s_ll_delta.y+s_ll_delta.x)*x->mslist[1]/rect.width;
            break;
        case 4:
            atom_setfloat(&cx,pt.x*x->mslist[1]/rect.width+x->mslist[0]);
            outlet_anything(x->ll_box.b_ob.o_outlet, gensym("mspos"), 1,&cx );
            return;
            break;
        default: break;
    }
    ll_mcwaveform_bang(x);
}


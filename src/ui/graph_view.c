#include "graph_view.h"
#include "../core/package.h"
#include <cairo/cairo.h>
#include <graphviz/gvc.h>
#include <math.h>

// Forward declarations
void draw_edge(cairo_t* cr, Agedge_t* e);
void draw_node(cairo_t* cr, Agnode_t* n, const char* selected);
static void update_graph(VenvGraphView* self);

struct _VenvGraphView {
    GtkWidget parent_instance;
    
    VenvAnalyzer* analyzer;
    GVC_t* gvc;
    Agraph_t* graph;
    double scale;
    double translate_x;
    double translate_y;
    char* selected_node;
    
    // Add drag state
    double drag_start_x;
    double drag_start_y;
};

G_DEFINE_TYPE(VenvGraphView, venv_graph_view, GTK_TYPE_WIDGET)

void venv_graph_view_snapshot(GtkWidget* widget, GtkSnapshot* snapshot) {
    VenvGraphView* self = VENV_GRAPH_VIEW(widget);
    graphene_rect_t bounds;
    if (!gtk_widget_compute_bounds(widget, widget, &bounds))
        return;

    // Fill background
    gtk_snapshot_append_color(snapshot,
                            &(GdkRGBA){.red = 1.0, .green = 1.0, .blue = 1.0, .alpha = 1.0},
                            &bounds);

    // Create cairo context from snapshot
    cairo_t* cr = gtk_snapshot_append_cairo(snapshot,
                                          &GRAPHENE_RECT_INIT(
                                              bounds.origin.x,
                                              bounds.origin.y,
                                              bounds.size.width,
                                              bounds.size.height));

    // Set up transform
    cairo_translate(cr, self->translate_x, self->translate_y);
    cairo_scale(cr, self->scale, self->scale);

    // Draw graph
    if (self->graph) {
        // Draw edges first
        for (Agnode_t* n = agfstnode(self->graph); n; n = agnxtnode(self->graph, n)) {
            for (Agedge_t* e = agfstedge(self->graph, n); e; e = agnxtedge(self->graph, e, n)) {
                draw_edge(cr, e);
            }
        }

        // Draw nodes on top
        for (Agnode_t* n = agfstnode(self->graph); n; n = agnxtnode(self->graph, n)) {
            draw_node(cr, n, self->selected_node);
        }
    }

    cairo_destroy(cr);
}

void draw_edge(cairo_t* cr, Agedge_t* e) {
    // Get edge points
    splines* spl = ED_spl(e);
    if (!spl) return;
    
    bezier* bz = &spl->list[0];
    if (bz->size < 4) return;
    
    // Draw edge path
    cairo_new_path(cr);
    cairo_move_to(cr, bz->list[0].x, bz->list[0].y);
    
    for (int i = 1; i < bz->size; i += 3) {
        cairo_curve_to(cr,
                      bz->list[i].x, bz->list[i].y,
                      bz->list[i+1].x, bz->list[i+1].y,
                      bz->list[i+2].x, bz->list[i+2].y);
    }
    
    cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.8);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
}

void draw_node(cairo_t* cr, Agnode_t* n, const char* selected) {
    pointf pos = ND_coord(n);
    double w = ND_width(n) * 72;  // Convert to points
    double h = ND_height(n) * 72;
    
    // Draw node background
    cairo_save(cr);
    cairo_translate(cr, pos.x, pos.y);
    
    // Fill background
    if (selected && strcmp(selected, agnameof(n)) == 0) {
        cairo_set_source_rgb(cr, 0.9, 0.9, 1.0);
    } else if (agget(n, "color") && strcmp(agget(n, "color"), "red") == 0) {
        cairo_set_source_rgb(cr, 1.0, 0.9, 0.9);
    } else {
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    }
    
    cairo_rectangle(cr, -w/2, -h/2, w, h);
    cairo_fill_preserve(cr);
    
    // Draw border
    cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
    
    // Draw label
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12.0);
    
    cairo_text_extents_t extents;
    const char* name = agnameof(n);
    cairo_text_extents(cr, name, &extents);
    
    cairo_move_to(cr, -extents.width/2, extents.height/2);
    cairo_show_text(cr, name);
    
    cairo_restore(cr);
}

static void
venv_graph_view_dispose(GObject* object)
{
    VenvGraphView* self = VENV_GRAPH_VIEW(object);
    
    if (self->graph) {
        gvFreeLayout(self->gvc, self->graph);
        agclose(self->graph);
        self->graph = NULL;
    }
    
    if (self->gvc) {
        gvFreeContext(self->gvc);
        self->gvc = NULL;
    }
    
    g_free(self->selected_node);
    self->selected_node = NULL;
    
    G_OBJECT_CLASS(venv_graph_view_parent_class)->dispose(object);
}

static void
venv_graph_view_class_init(VenvGraphViewClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    
    object_class->dispose = venv_graph_view_dispose;
    widget_class->snapshot = venv_graph_view_snapshot;
    gtk_widget_class_set_layout_manager_type(widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void update_graph(VenvGraphView* self) {
    if (!self->analyzer) return;
    
    if (self->graph) {
        gvFreeLayout(self->gvc, self->graph);
        agclose(self->graph);
    }
    
    self->graph = agopen("deps", Agdirected, NULL);
    agsafeset(self->graph, "rankdir", "LR", "");
    
    // Create nodes
    for (Package* pkg = self->analyzer->packages; pkg; pkg = pkg->next) {
        Agnode_t* node = agnode(self->graph, pkg->name, TRUE);
        if (pkg->conflicts) {
            agsafeset(node, "color", "red", "black");
        }
    }
    
    // Create edges
    for (Package* pkg = self->analyzer->packages; pkg; pkg = pkg->next) {
        for (PackageDep* dep = pkg->dependencies; dep; dep = dep->next) {
            Agnode_t* from = agfindnode(self->graph, pkg->name);
            Agnode_t* to = agfindnode(self->graph, dep->name);
            if (from && to) {
                agedge(self->graph, from, to, NULL, TRUE);
            }
        }
    }
    
    // Layout
    gvLayout(self->gvc, self->graph, "dot");
    
    // Request redraw
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

static void on_motion(GtkEventControllerMotion* controller G_GNUC_UNUSED,
                     double x, double y, 
                     gpointer data) {
    VenvGraphView* self = VENV_GRAPH_VIEW(data);
    
    // Hit testing for node selection
    if (self->graph) {
        x = (x - self->translate_x) / self->scale;
        y = (y - self->translate_y) / self->scale;
        
        for (Agnode_t* n = agfstnode(self->graph); n; n = agnxtnode(self->graph, n)) {
            pointf pos = ND_coord(n);
            double w = ND_width(n) * 36;
            double h = ND_height(n) * 36;
            
            if (x >= pos.x - w/2 && x <= pos.x + w/2 &&
                y >= pos.y - h/2 && y <= pos.y + h/2) {
                g_free(self->selected_node);
                self->selected_node = g_strdup(agnameof(n));
                gtk_widget_queue_draw(GTK_WIDGET(self));
                break;
            }
        }
    }
}

static void on_drag_begin(GtkGestureDrag* gesture G_GNUC_UNUSED,
                         double x, double y, 
                         gpointer data) {
    VenvGraphView* self = VENV_GRAPH_VIEW(data);
    self->drag_start_x = x - self->translate_x;
    self->drag_start_y = y - self->translate_y;
}

static void on_drag_update(GtkGestureDrag* gesture,
                          double offset_x G_GNUC_UNUSED, 
                          double offset_y G_GNUC_UNUSED,
                          gpointer data) {
    VenvGraphView* self = VENV_GRAPH_VIEW(data);
    double x, y;
    gtk_gesture_drag_get_offset(gesture, &x, &y);
    
    self->translate_x = x + self->drag_start_x;
    self->translate_y = y + self->drag_start_y;
    
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

static void on_scroll(GtkEventControllerScroll* controller,
                     double dx G_GNUC_UNUSED, double dy,
                     gpointer data) {
    VenvGraphView* self = VENV_GRAPH_VIEW(data);
    
    // Zoom around mouse position
    double x, y;
    GdkEvent* event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
    gdk_event_get_position(event, &x, &y);    
    
    double scale_delta = dy > 0 ? 0.9 : 1.1;
    double new_scale = self->scale * scale_delta;
    
    // Limit zoom range
    if (new_scale >= 0.1 && new_scale <= 5.0) {
        // Adjust translation to zoom around mouse point
        self->translate_x = x - (x - self->translate_x) * scale_delta;
        self->translate_y = y - (y - self->translate_y) * scale_delta;
        self->scale = new_scale;
        
        gtk_widget_queue_draw(GTK_WIDGET(self));
    }
}

static void venv_graph_view_init(VenvGraphView* self) {
    self->scale = 1.0;
    self->translate_x = 0;
    self->translate_y = 0;
    self->selected_node = NULL;
    self->gvc = gvContext();
    
    // Setup event controllers
    GtkEventController* motion = gtk_event_controller_motion_new();
    g_signal_connect(motion, "motion", G_CALLBACK(on_motion), self);
    gtk_widget_add_controller(GTK_WIDGET(self), motion);
    
    GtkGesture* drag = gtk_gesture_drag_new();
    g_signal_connect(drag, "drag-begin", G_CALLBACK(on_drag_begin), self);
    g_signal_connect(drag, "drag-update", G_CALLBACK(on_drag_update), self);
    gtk_widget_add_controller(GTK_WIDGET(self), GTK_EVENT_CONTROLLER(drag));
    
    GtkEventController* scroll = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
    g_signal_connect(scroll, "scroll", G_CALLBACK(on_scroll), self);
    gtk_widget_add_controller(GTK_WIDGET(self), scroll);
}

GtkWidget* venv_graph_view_new(VenvAnalyzer* analyzer) {
    VenvGraphView* view = g_object_new(VENV_TYPE_GRAPH_VIEW, NULL);
    view->analyzer = analyzer;
    update_graph(view);
    return GTK_WIDGET(view);
}

void venv_graph_view_update(GtkWidget* widget) {
    g_return_if_fail(VENV_IS_GRAPH_VIEW(widget));
    update_graph(VENV_GRAPH_VIEW(widget));
}
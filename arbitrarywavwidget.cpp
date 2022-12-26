#include "arbitrarywavwidget.h"
#include "bezier.h"


ArbitraryWavWidget::ArbitraryWavWidget(QWidget *parent)
    : QWidget{parent} {
    mirror_null = 0;
    mirror_radius = 12*25;  // 24 inch mirror by default
    wave_height=0.3;  // by default we start at +/- .25 waves of vertical scale
    ww_unit=in; // default inches
    double bez_dist = mirror_radius/bez_distance_ratio;
    pts.append(CPoint(0,0, bez_dist));
    pts.append(CPoint(25.4,0.125, bez_dist));
    // debugging test pts[1].setLeft(pts[1].x()-3, pts[1].y()-.3,0.1);
    bDragging=false;
    bDraggingBevierPoint = false;
}

void ArbitraryWavWidget::setMode(int _mode) {
    mode = _mode;
    update(); // redraw
    }

ArbitraryWavWidget::~ArbitraryWavWidget(){
    pts.empty();
    qDebug() << "wav widget destructor";
}

bool comparePoints(const QPointF &a, const QPointF &b) {
    return a.x() < b.x();
}

void ArbitraryWavWidget::sortPoints() {
     std::sort(pts.begin(), pts.end(), comparePoints);
}

QSize ArbitraryWavWidget::sizeHint() const {
    return QSize(1400,400);
}
QSize ArbitraryWavWidget::minimumSizeHint() const {
    return QSize(200,100);

}

void ArbitraryWavWidget::resizeEvent(QResizeEvent * /*event*/) {
    QSize sz = size();
    height = sz.height();
    width = sz.width();

    graph_left=45; // where actual lines are being drawn - this is also pixels reserved for y axis labels on left side
    graph_height=height-22; // pixels reserved for actual graph - height-graph_height is reserved for x axis labels at bottom
    pos_y0 = graph_height/2;
    pos_edge = width - (width-graph_left)*0.05; // .05 means 5% reserved to go beyond edge of mirror
}

// these next 4 functions: transx translates distance in mm to position in our graph and back
//                         transy translates wavelength to position in our graph and back
int ArbitraryWavWidget::transx(double r){
    return (r/mirror_radius)*(pos_edge-graph_left) + graph_left;
}
int ArbitraryWavWidget::transy(double y){
    // -y because positive pixels is in the negative y direction (more positive pixels towards bottom of graph)
    return -y/wave_height*(graph_height-pos_y0) + pos_y0;
}
double ArbitraryWavWidget::transx(int x){
    return (x-graph_left)*mirror_radius/(pos_edge-graph_left);
}
double ArbitraryWavWidget::transy(int y){
    return (y-pos_y0)*(-wave_height)/(graph_height-pos_y0);
}

double ArbitraryWavWidget::transRatio(){
    return fabs((wave_height)/(graph_height-pos_y0) / (mirror_radius/(pos_edge-graph_left)));
}

int ArbitraryWavWidget::findPoint(QPoint p1) {
    for(int i=0; i<pts.size(); i++) {
        QPointF p2 = pts.at(i);
        if ( (abs(p1.x() - transx(p2.x())) < 8) &&
             (abs(p1.y() - transy(p2.y())) < 8) ) {
            return i;
        }
    }
    return -1; // not found
}

void ArbitraryWavWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        dragging_point_index = findPoint(event->pos());
        if (dragging_point_index >=0) {
            bDragging=true;
            return;
        }
        if (mode == 0) {
            // we are in bezier mode.  Search those control points now...
            //dragging_bev_index = findBev(event->pos());
            for(int i=0; i<pts.size(); i++) {
                QPoint const &p1 = event->pos();
                CPoint main_point = pts.at(i);

                QPointF p2 = main_point.getLeft();
                if ( (abs(p1.x() - transx(p2.x())) < 8) &&
                     (abs(p1.y() - transy(p2.y())) < 8) ) {
                    // found it
                    dragging_point_index = i;
                    bDragging_bev_left = true;
                    bDraggingBevierPoint=true;
                    return;
                }

                p2 = main_point.getRight();
                if ( (abs(p1.x() - transx(p2.x())) < 8) &&
                     (abs(p1.y() - transy(p2.y())) < 8) ) {
                    // found it
                    dragging_point_index = i;
                    bDragging_bev_left = false; // it's the right point
                    bDraggingBevierPoint=true;
                    return;
                }
            }
        }
        // let's create a new point here
        QPoint ip = event->pos();
        CPoint newp(transx((int)ip.x()), transy((int)ip.y()), mirror_radius/bez_distance_ratio);
        pts.append(newp);
        sortPoints();
        dragging_point_index = findPoint(event->pos());
        // default handles to half way in X of nearest two points
        if (dragging_point_index>0) {
            // left handle of new point
            CPoint &myPoint = pts[dragging_point_index];
            CPoint &leftPoint = pts[dragging_point_index-1];
            double desired_length = fabs(myPoint.x() - leftPoint.x()) / 2;
            myPoint.lx = myPoint.x() - desired_length;
            // right handle of prev point
            leftPoint.setRight(leftPoint.x()+desired_length, leftPoint.ry,transRatio());
        }
        if (dragging_point_index == pts.size()-1) {
            // added point is last point in list
            CPoint &myPoint = pts[dragging_point_index];
            myPoint.rx = myPoint.x() + 10; // last point - just make handle 10mm out.  Always
        } else {
            // more points to the right
            CPoint &myPoint = pts[dragging_point_index];
            CPoint &nextPoint = pts[dragging_point_index+1];
            double desired_length = fabs(myPoint.x() - nextPoint.x()) / 2;
            myPoint.rx = myPoint.x() + desired_length;
            nextPoint.setLeft(nextPoint.x() - desired_length, nextPoint.ly, transRatio());
        }

        if (dragging_point_index>=0)
            bDragging=true; // hopefully it always gets here
        update();
    } else {
        if (event->button() == Qt::RightButton) {
            // delete point (if mouse over point)
            int i = findPoint(event->pos());
            if (i>0) // can't delete point 0 - it's permanent
                pts.removeAt(i);
            update();
        }
    }
}
void ArbitraryWavWidget::mouseMoveEvent(QMouseEvent *event) {
    if (bDragging == false && bDraggingBevierPoint == false )
        return;

    if ( (event->buttons() & Qt::LeftButton) == 0)
        return;

    // update position of point
    QPoint ip = event->pos();

    if (bDragging) {
        pts[dragging_point_index].setX(transx(ip.x()));
        pts[dragging_point_index].setY(transy(ip.y()));

        if (dragging_point_index == 0)
            pts[dragging_point_index].setX(0); // this point always at mirror center and can't be moved sideways
        if (ip.x() > width)
            pts[dragging_point_index].setX(transx(width)); // don't allow points to go outside graph area on right side
        if (ip.x() < graph_left)
            pts[dragging_point_index].setX(0); // don't allow points to go outside graph area on left side


        //qDebug() << "x " << ip.x() << " y " << ip.y() << " tx " << transx(ip.x()) << " ty " << transy(ip.y());
        sortPoints();// this messes up which point is being dragged so re-locate the point
        int i = findPoint(event->pos());
        if (i>=0) {
            dragging_point_index=i;
            fixOverhangs(i);
        }
    }
    if (bDraggingBevierPoint) {
        double x = transx(ip.x());
        double y = transy(ip.y());
        if (dragging_point_index == 0)
            y = pts[0].y(); // first point (pts[0]) bez control point forced to be horizontal
        if (bDragging_bev_left)
            pts[dragging_point_index].setLeft(x,y,transRatio());
        else
            pts[dragging_point_index].setRight(x,y,transRatio());
        fixOverhangs(dragging_point_index);
    }

    update(); // redraw
}

void ArbitraryWavWidget::fixOverhangs(int index) {
    if (bDissuadeOverhangs == false)
        return;
    if (index < 0 || index >= pts.size())
        return;

    if (index>0) {
        // left handle of point to fix
        CPoint &myPoint = pts[index];
        CPoint &leftPoint = pts[index-1];
        double max_combined_length = fabs(myPoint.x() - leftPoint.x()) * 2;
        double combined_length = myPoint.x()-myPoint.lx +
                                leftPoint.rx - leftPoint.x();
        if (combined_length > max_combined_length) {
            // scale back both bez handles in x distance
            double scaleback = max_combined_length/combined_length;
            myPoint.setLeft(myPoint.x() - (myPoint.x() - myPoint.lx)*scaleback, myPoint.ly, transRatio());
            leftPoint.setRight(leftPoint.x() + (leftPoint.rx-leftPoint.x())*scaleback, leftPoint.ry, transRatio());
        }
    }
    if (index == pts.size()-1) {
        // point is last point in list - don't mess with right handle
    } else {
        // more points to the right
        CPoint &myPoint = pts[index];
        CPoint &nextPoint = pts[index+1];
        double max_combined_length = fabs(nextPoint.x() - myPoint.x()) * 2;
        double combined_length = nextPoint.x()-nextPoint.lx +
                                myPoint.rx - myPoint.x();
        if (combined_length > max_combined_length) {
            // scale back both bez handles in x distance
            double scaleback = max_combined_length/combined_length;
            myPoint.setRight(myPoint.x() + (myPoint.rx - myPoint.x())*scaleback, myPoint.ry, transRatio());
            nextPoint.setLeft(nextPoint.x() - (nextPoint.x()-nextPoint.lx)*scaleback, nextPoint.ly, transRatio());
        }
    }



}

void ArbitraryWavWidget::mouseReleaseEvent(QMouseEvent * event) {
    bDragging = false;
    bDraggingBevierPoint = false;
    int index = findPoint(event->pos());
    fixOverhangs(index);
}

void ArbitraryWavWidget::wheelEvent(QWheelEvent *event) {
    //qDebug() << "wheel event" << event->angleDelta().y();
    if (event->angleDelta().y() > 0) {
        // scroll up.  zoom in.
        wave_height /= 1.1;
    }
    if (event->angleDelta().y() < 0) {
        // scroll down.  zoom out.
        wave_height *= 1.1;
    }
    update(); // redraw
    event->accept();
}

void ArbitraryWavWidget::doCurve(QPainter &painter, int left_point_x, int left_point_y, int right_point_x, int right_point_y) {
    // do a cubic double curve (an S curve) such that slope is zero at each end
    // Formula is basically in the form of y=jerk*x^3 for each of the 2 curves
    // I think of y=a*x^2 as a motion where a is acceleration.  For cubics the term is called
    // jerk (then snap, crackle, pop or sometimes snap is called jounce)

    double half_dist = (right_point_x - left_point_x)/2; // horizontal distance to fill with each curve
    double half_height = (right_point_y - left_point_y)/2; // vertical distance to fill with each curve
    // y=jerk*(x-left_point_x)^3+left_point_y
    double jerk = half_height/pow(half_dist,3);
    int ix_prev=0, iy_prev=0;
    bool bFirst;
    // left half of curve
    bFirst=true;
    for (int ix = left_point_x; ix <= left_point_x+half_dist; ix++) {
        double cubed = ix - left_point_x;
        cubed = cubed*cubed*cubed;
        int iy = jerk*cubed+left_point_y+0.5; // 0.5 is for rounding to nearest integer
        if (bFirst) {
            bFirst=false;
            painter.drawPoint(ix, iy);
        }
        else
            painter.drawLine(ix_prev,iy_prev,ix,iy);
        ix_prev=ix;
        iy_prev=iy;
    }
    // right half of curve
    for (int ix = left_point_x+half_dist+1; ix <= right_point_x; ix++) {
        double cubed = right_point_x - ix;
        cubed = cubed*cubed*cubed;
        int iy = right_point_y-jerk*cubed+0.5; // 0.5 is for rounding to nearest integer
        if (bFirst) {
            // seems impossible but possibly the earlier loop never had any points
            bFirst=false;
            painter.drawPoint(ix, iy);
        }
        else
            painter.drawLine(ix_prev,iy_prev,ix,iy);
        ix_prev=ix;
        iy_prev=iy;
    }

}

void ArbitraryWavWidget::paintEvent(QPaintEvent * /*event*/) {


    QPainter painter(this);
    QPen grayPen(Qt::gray, 1, Qt::SolidLine);
    QPen blackPen(Qt::black, 1, Qt::SolidLine);
    QPen bluePen(Qt::blue, 1.5, Qt::SolidLine);

    //painter.drawLine(1,1,100,100);



    //qDebug() << "h: " << height << " w: " << width;
    // border
    painter.drawLine(0,0,0,height-1);
    painter.drawLine(0,height-1,width-1,height-1);
    painter.drawLine(width-1,height-1,width-1,0);
    painter.drawLine(width-1,0,0,0);

    // graph grid
    double y=0;
    int iy;
    // show horizontal lines.  First determine spacing.
    double vertical_spacing=0.125; // 1/8 wave - default
    if (wave_height/.125 > 12)
        vertical_spacing=1;
    else if (wave_height/.125 > 6)
        vertical_spacing=0.25;

    painter.setFont(QFont("Arial",10));
    while(y < wave_height) {
        if (y==0)
            painter.setPen(blackPen);
        else
            painter.setPen(grayPen);

        iy = transy(y);
        painter.drawLine(graph_left, iy, pos_edge, iy);

        // text
        QRect rect1(2,iy-8, graph_left, 20);
        painter.drawText(rect1,QString{ "%1" }.arg(y, 0, 'f', 3));

        iy = transy(-y);
        painter.drawLine(graph_left, iy, pos_edge, iy);

        // text
        QRect rect2(2,iy-8, graph_left, 20);
        painter.drawText(rect2,QString{ "%1" }.arg(-y, 0, 'f', 3));



        y += vertical_spacing;
    }

    // determine frequency of vertical lines
    double spacing=1; // always in mm
    switch(ww_unit)
    {
        case in:
            spacing=25.4;
            break;
        case cm:
        case mm:
            spacing=10;
            break;
    }

    // now reduce the number of vertical lines - we want at least 150 pixels between lines
    int pix_dist = transx(spacing);
    int count=1;
    while (pix_dist < 150) { // number here is minimum pixels per vertical line
        count++;
        pix_dist = transx(spacing*count);
    }
    spacing *= count;
    qDebug() << "spacing: " << spacing;

    // left edge of graph
    int ix;
    ix = pos_edge;
    painter.drawLine(ix,0,ix,graph_height);


    double x=0;
    while(x <= mirror_radius) {
        if (x==0)
            painter.setPen(blackPen);
        else
            painter.setPen(grayPen);
        ix = transx(x);
        painter.drawLine(ix,0,ix,graph_height);

        // text
        QString s;
        switch (ww_unit) {
        case mm:
            s = QString{ "%1 mm"}.arg(x, 0, 'f', 0);
            break;
        case cm:
            s = QString{ "%1 cm"}.arg(x/10, 0, 'f', 0);
            break;
        case in:
        default:
            s = QString{ "%1 in"}.arg(x/25.4, 0, 'f', 1);
            break;
        }

        QRect rect2(ix-25,graph_height+1, 50, 20);
        painter.drawText(rect2, Qt::AlignCenter, s);

        x+=spacing;
    }

    // draw the control points
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(QBrush(Qt::gray, Qt::SolidPattern));
    painter.setPen(grayPen);
    for(int i=0; i<pts.size(); i++) {
        CPoint p = pts.at(i);
        if (this->mode==0) {
            // bezier mode - draw the control points
            //left
            QPointF pbez = p.getLeft();
            if (i>0) {
                // don't draw left of first point - that control point not used
                painter.drawLine(transx(p.x()), transy(p.y()), transx(pbez.x()), transy(pbez.y()) );
                painter.drawRect(transx(pbez.x())-3, transy(pbez.y())-2, 4, 4);
            }
            //right
            pbez = p.getRight();
            painter.drawLine(transx(p.x()), transy(p.y()), transx(pbez.x()), transy(pbez.y()) );
            painter.drawRect(transx(pbez.x())-3, transy(pbez.y())-2, 4, 4);
        }
        // do the main point after the bez lines
        painter.drawEllipse(transx(p.x())-5, transy(p.y())-4, 8, 8);  // 10,10 is the width/height of the circle;  -5 is because this function tells where corner goes
    }


    // draw the curves
    painter.setPen(bluePen);

    if (this->mode == 1) {
        // cubic mode
        QPointF leftPoint = pts.at(0);
        int left_point_x = transx(leftPoint.x());
        int left_point_y = transy(leftPoint.y());
        int right_point_x=0, right_point_y=0;
        for(int i=1; i<pts.size(); i++) {
            QPointF p = pts.at(i);
            right_point_x = transx(p.x());
            right_point_y = transy(p.y());

            doCurve(painter, left_point_x, left_point_y, right_point_x, right_point_y);

            left_point_x=right_point_x;
            left_point_y=right_point_y;
        }

        if (right_point_x < transx(mirror_radius)) {
            doCurve(painter, left_point_x, left_point_y, width, left_point_y);
        }
    } else if (this->mode==0) {
        // bezier mode
        Bezier::Point bez_pts[4];
        for(int i=1; i<pts.size(); i++) { // start at 1 and we will grab 2 points at a time
            CPoint p1 = pts.at(i-1);
            CPoint p4 = pts.at(i);
            QPointF p2 = p1.getRight();
            QPointF p3 = p4.getLeft();

            bez_pts[0]= Bezier::Point(p1.x(), p1.y());
            bez_pts[1]= Bezier::Point(p2.x(), p2.y());
            bez_pts[2]= Bezier::Point(p3.x(), p3.y());
            bez_pts[3]= Bezier::Point(p4.x(), p4.y());

            Bezier::Bezier<3> bez(bez_pts,4);
            Bezier::Point p_on_bez1, p_on_bez2;
            p_on_bez1 = bez.valueAt(0);
            const double t_increment = .01;
            for (double t=t_increment; t<=1; t+=t_increment) {
                p_on_bez2 = bez.valueAt(t);
                if (p_on_bez1.x > mirror_radius)
                    painter.setPen(grayPen);
                painter.drawLine(transx(p_on_bez1.x),transy(p_on_bez1.y),transx(p_on_bez2.x),transy(p_on_bez2.y));
                p_on_bez1 = p_on_bez2;
            }
        }

    }




}

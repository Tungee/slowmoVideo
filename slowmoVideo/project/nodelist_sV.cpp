/*
slowmoUI is a user interface for slowmoVideo.
Copyright (C) 2011  Simon A. Eugster (Granjow)  <simon.eu@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "nodelist_sV.h"
#include "node_sV.h"

#include <cmath>

#include <QDebug>

NodeList_sV::NodeList_sV(float minDist) :
    m_maxY(10),
    m_list(),
    m_minDist(minDist)
{
}

void NodeList_sV::setMaxY(qreal time)
{
    Q_ASSERT(time > 0);
    m_maxY = time;
}

inline
qreal NodeList_sV::startTime() const
{
    if (m_list.length() > 0) {
        return m_list[0].xUnmoved();
    } else {
        qDebug() << "No start time available (no nodes)";
        return 0;
    }
}
inline
qreal NodeList_sV::endTime() const
{
    if (m_list.length() > 0) {
        return m_list[m_list.length()-1].xUnmoved();
    } else {
        qDebug() << "No end time available (no nodes)";
        return 0;
    }
}
qreal NodeList_sV::totalTime() const
{
    return endTime()-startTime();
}
qreal NodeList_sV::sourceTime(qreal targetTime) const
{
    qreal srcTime = -1;
    int index = find(targetTime);
    if (index >= 0) {
        if (m_list.size() > index) {
            float ratio = (targetTime-m_list[index].x())/(m_list[index+1].x()-m_list[index].x());
            srcTime = m_list[index].y() + ratio*( m_list[index+1].y()-m_list[index].y() );
        } else {
            Q_ASSERT(false);
            srcTime = m_list[index].y();
        }
    } else {
        qDebug() << "No node before " << targetTime;
        Q_ASSERT(false);
        if (m_list.size() > 0) {
            srcTime = m_list[0].y();
        }
    }
    return srcTime;
}

bool NodeList_sV::add(const Node_sV node)
{
    bool add = true;

    int pos = find(node.x());
    if (pos >= 0 && m_list.size() > pos) {
        add = fabs(node.x()-m_list.at(pos).x()) > m_minDist;
        qDebug() << "Left distance is " << fabs(node.x()-m_list.at(pos).x());
        if (add && m_list.size() > pos+1) {
            add = fabs(node.x()-m_list.at(pos+1).x()) > m_minDist;
            qDebug() << "Right distance is " << fabs(node.x()-m_list.at(pos+1).x());
        }
    }
    qDebug() << "Adding? " << add;
    if (add) {
        m_list.append(node);
        qSort(m_list);
    }

    return add;
}

uint NodeList_sV::deleteSelected()
{
    uint counter = 0;
    for (int i = 0; i < m_list.size(); ) {
        if (m_list.at(i).selected()) {
            m_list.removeOne(m_list.at(i));
            counter++;
        } else {
            i++;
        }
    }
    return counter;
}

void NodeList_sV::unselectAll()
{
    for (int i = 0; i < m_list.size(); i++) {
        m_list[i].select(false);
    }
}


bool NodeList_sV::validate() const
{
    bool valid = true;
    qreal last = -m_minDist;
    for (int i = 0; i < m_list.size() && valid; i++) {
        valid = m_list.at(i).x() >= 0
                && m_list.at(i).x() - last >= m_minDist
                && m_list.at(i).y() >= 0;
        last = m_list.at(i).x();
        if (!valid) {
            qDebug() << "Invalid node position for node " << i << "; Distance is " << m_list.at(i).x() - last;
        }
        Q_ASSERT(valid);
    }
    return valid;
}


////////// Moving

void NodeList_sV::moveSelected(const Node_sV &time)
{
    qreal maxRMove = 1000;
    qreal maxLMove = -1000;
    qreal maxUMove = 1000;
    qreal maxDMove = -1000;
    const Node_sV *left = NULL;
    const Node_sV *right;
    for (int i = 0; i < m_list.size(); i++) {
        right = &m_list.at(i);

        /*
          Get the maximum allowed horizontal movement distance here such that there is no
          overlapping. For moving the selected nodes to the left, only unselected nodes
          which are directly followed by a selected node need to be taken into account.
                 O----O
                /      \               x
          -----x        \             /
                         x-----------O

         min(  ^1^,      ^-----2-----^    ) + minDist

         */
        if (left != NULL) {
            if (left->selected() && !right->selected()) {
                // Move-right distance
                maxRMove = qMin(maxRMove, right->xUnmoved() - left->xUnmoved() - m_minDist);
            } else if (!left->selected() && right->selected()) {
                // Move-left distance
                maxLMove = qMax(maxLMove, left->xUnmoved() - right->xUnmoved() + m_minDist);
            }
        }

        if (right->selected()) {
            maxUMove = qMin(maxUMove, m_maxY - right->yUnmoved());
            maxDMove = qMax(maxDMove, 0-right->yUnmoved());
        }

        left = right;
    }
    if (m_list.size() > 0 && m_list.at(0).selected()) {
        // Do not allow to move nodes to x < 0
        maxLMove = qMax(maxLMove, -m_list.at(0).xUnmoved());
    }
    qDebug() << "Max move: left " << maxLMove << ", right: " << maxRMove;
    if (maxLMove <= time.x() && time.x() <= maxRMove
            && maxDMove <= time.y() && time.y() <= maxUMove) {
        for (int i = 0; i < m_list.size(); i++) {
            if (m_list.at(i).selected()) {
                m_list[i].move(time);
            }
        }
    } else {
        qDebug() << "Not within valid range:" << time;
    }
}
void NodeList_sV::shift(qreal after, qreal by)
{
    int pos = nodeAfter(after);
    if (pos >= 0) {
        if (pos > 0) {
            qDebug() << "Max of " << by << " and " << m_list.at(pos-1).xUnmoved() - m_list.at(pos).xUnmoved() + m_minDist;
            by = qMax(by, m_list.at(pos-1).xUnmoved() - m_list.at(pos).xUnmoved() + m_minDist);
        }
        if (pos == 0) {
            by = qMax(by, -m_list.at(pos).xUnmoved());
        }
        for (; pos < m_list.size(); pos++) {
            m_list[pos].move(Node_sV(by, 0));
        }
    }
    if (!validate()) {
        qDebug() << "Invalid node configuration! (This should not happen.)";
    }
}

void NodeList_sV::confirmMove()
{
    for (int i = 0; i < m_list.size(); i++) {
        m_list[i].confirmMove();
    }
    validate();
}
void NodeList_sV::abortMove()
{
    for (int i = 0; i < m_list.size(); i++) {
        if (m_list.at(i).selected()) {
            m_list[i].abortMove();
        }
    }
}




////////// Access

int NodeList_sV::find(qreal time) const
{
    uint pos;
    for (
         pos = 0;
         m_list.size() > (pos+1) && m_list.at(pos+1).x() <= time;
         pos++
         ) {}
    if (m_list.size() == 0 || (pos == 0 && m_list[pos].x() > time)) {
        pos = -1;
    }
    return pos;
}

int NodeList_sV::nodeAfter(qreal time) const
{
    int pos = 0;
    while (m_list.size() > pos) {
        if (m_list.at(pos).xUnmoved() >= time) {
            break;
        }
        pos++;
    }
    if (pos >= m_list.size()) {
        pos = -1;
    }
    Q_ASSERT(pos < 0 || m_list.at(pos).xUnmoved() >= time);
    return pos;
}
const Node_sV* NodeList_sV::near(qreal t) const
{
    for (int i = 0; i < m_list.size(); i++) {
        if (fabs(m_list.at(i).x() - t) < m_minDist) {
            return &m_list.at(i);
        }
    }
    return NULL;
}
const Node_sV& NodeList_sV::at(int i) const { return m_list.at(i); }
Node_sV& NodeList_sV::operator[](int i) { return m_list[i]; }
int NodeList_sV::size() const { return m_list.size(); }

QDebug operator<<(QDebug dbg, const NodeList_sV &list)
{
    for (int i = 0; i < list.size(); i++) {
        dbg.nospace() << list.near(i) << " ";
    }
    return dbg.maybeSpace();
}
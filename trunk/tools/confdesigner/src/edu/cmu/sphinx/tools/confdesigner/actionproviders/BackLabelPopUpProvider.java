package edu.cmu.sphinx.tools.confdesigner.actionproviders;

import edu.cmu.sphinx.tools.confdesigner.ConfigScene;
import org.netbeans.api.visual.action.PopupMenuProvider;
import org.netbeans.api.visual.widget.LabelWidget;
import org.netbeans.api.visual.widget.Widget;

import javax.swing.*;
import java.awt.*;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;

/**
 * A little popup which is used to configure background label areas.
 *
 * @author Holger Brandl
 */
public class BackLabelPopUpProvider implements PopupMenuProvider {

    public final String REMOVE_LABEL = "remove";
    public final String SET_COLOR_LABEL = "set color";
    private LabelWidget parent;


    public BackLabelPopUpProvider(LabelWidget parent) {
        assert parent != null;
        this.parent = parent;
    }


    public JPopupMenu getPopupMenu(final Widget widget, Point point) {
        JPopupMenu menu = new JPopupMenu("test");

        ActionListener al = new ActionListener() {

            public void actionPerformed(ActionEvent e) {
                JMenuItem sourceItem = (JMenuItem) e.getSource();

                if (sourceItem.getText().equals(REMOVE_LABEL)) {
                    ((ConfigScene) parent.getScene()).removeBckndLabel(parent);
                } else if (sourceItem.getText().equals(SET_COLOR_LABEL)) {
                    //todo implement me
                }
            }
        };
        JMenuItem item = new JMenuItem(REMOVE_LABEL);
        item.addActionListener(al);
        menu.add(item);

        item = new JMenuItem(SET_COLOR_LABEL);
        item.setEnabled(false);
        item.addActionListener(al);
        menu.add(item);

        return menu;
    }

}



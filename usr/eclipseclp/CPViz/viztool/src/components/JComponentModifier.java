/*

   Licensed to the Apache Software Foundation (ASF) under one or more
   contributor license agreements.  See the NOTICE file distributed with
   this work for additional information regarding copyright ownership.
   The ASF licenses this file to You under the Apache License, Version 2.0
   (the "License"); you may not use this file except in compliance with
   the License.  You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

 */
package components;

import javax.swing.JComponent;

/**
 * This interface must be implemented by actions which need
 * to have an access to their associated component(s)
 *
 * @author <a href="mailto:stephane@hillion.org">Stephane Hillion</a>
 * @version $Id: JComponentModifier.java,v 1.1 2013/02/21 11:24:35 jschimpf Exp $
 */
public interface JComponentModifier {
    /**
     * Gives a reference to a component to this object
     * @param comp the component associed with this object
     */
    void addJComponent(JComponent comp);
}
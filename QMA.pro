# /* ----------------------------------------------------------------- */
# /*                                                                   */
# /*  Copyright (c) 2010-2011  hkrn (libMMDAI)                         */
# /*                                                                   */
# /* All rights reserved.                                              */
# /*                                                                   */
# /* Redistribution and use in source and binary forms, with or        */
# /* without modification, are permitted provided that the following   */
# /* conditions are met:                                               */
# /*                                                                   */
# /* - Redistributions of source code must retain the above copyright  */
# /*   notice, this list of conditions and the following disclaimer.   */
# /* - Redistributions in binary form must reproduce the above         */
# /*   copyright notice, this list of conditions and the following     */
# /*   disclaimer in the documentation and/or other materials provided */
# /*   with the distribution.                                          */
# /* - Neither the name of the MMDAgent project team nor the names of  */
# /*   its contributors may be used to endorse or promote products     */
# /*   derived from this software without specific prior written       */
# /*   permission.                                                     */
# /*                                                                   */
# /* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND            */
# /* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,       */
# /* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF          */
# /* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE          */
# /* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS */
# /* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,          */
# /* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED   */
# /* TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,     */
# /* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON */
# /* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,   */
# /* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY    */
# /* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE           */
# /* POSSIBILITY OF SUCH DAMAGE.                                       */
# /* ----------------------------------------------------------------- */

QT += core gui opengl

TARGET = QtMMDAI
TEMPLATE = app

unix:LIBS += -L/usr/local/lib
unix:INCLUDEPATH += /usr/include/bullet /usr/local/include/bullet
macx:CONFIG:release += x86 x86_64

LIBS += -lMMDAI -lMMDME -lglee -lBulletDynamics -lBulletCollision -lLinearMath

# unused (using framework)
#
# deploy command is macdeployqt
#
# macx {
#     QMAKE_LFLAGS += -F../Library_MMDFiles
#     LIBS = -framework MMDFiles \
#            -L/usr/local/lib -lBulletSoftBody -lBulletDynamics -lBulletCollision -lLinearMath -lglee
#     INCLUDEPATH += ../Library_MMDFiles/include /usr/local/include/bullet
# }

SOURCES += main.cc\
        QMAWidget.cc \
        QMATimer.cc \
    QMAWindow.cc \
    QMAModelLoader.cc \
    QMAModelLoaderFactory.cc

HEADERS  += QMAWidget.h \
    QMAPlugin.h \
    QMATimer.h \
    QMAWindow.h \
    CommandDispatcher.h \
    QMAModelLoader.h \
    QMAModelLoaderFactory.h

TRANSLATIONS += res/translations/QMA_ja.ts

CODECFORTR = UTF-8

/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2017      Bianca Tost
**
**  Computer Graphics and Visualization Group
**  Technische Universitaet Muenchen, Garching, Germany
**
**  Met.3D is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  Met.3D is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with Met.3D.  If not, see <http://www.gnu.org/licenses/>.
**
*******************************************************************************/
#ifndef NWPMULTIVARACTOR_H
#define NWPMULTIVARACTOR_H

// standard library imports

// related third party imports
#include <QtCore>
#include <QtProperty>

// local application imports
#include "gxfw/mactor.h"
#include "gxfw/nwpactorvariable.h"
#include "gxfw/synccontrol.h"
#include "gxfw/selectdatasourcedialog.h"
#include "data/weatherpredictiondatasource.h"
#include "data/abstractanalysis.h"
#include "actors/transferfunction1d.h"

namespace Met3D
{

/**
  @brief MNWPMultiVarActor serves as base class for actors that visualise
  mulitple 3D NWP fields.

  The class acts as an intermediate class between @ref MActor and the actual
  actor implementations. As a subclass of @ref MActor, it implements logic to
  request data fields from a data loader, and manages forecast time.
  */
class MNWPMultiVarActor : public MActor
{
    Q_OBJECT

public:
    MNWPMultiVarActor();

    ~MNWPMultiVarActor();

    /**
     Tells every variable that is connected to an @ref MSyncControl instance to
     call the scene's variableSynchronizesWith() method. This is necessary if
     the actor is added to a scene during program runtime (that is, the
     synchronization has been established before the actor is added to the
     scene; in this case the scene needs to be informed about the connection).
     */
    void provideSynchronizationInfoToScene(MSceneControl *scene);

    const QList<MNWPActorVariable*>& getNWPVariables() const
    { return variables; }

    /**
      Connect an analysis control to this actor.
     */
    void setAnalysisControl(MAnalysisControl *a) { analysisControl = a; }

    /**
      Needs to be implemented in derived classes.

      Returns a list of the vertical level types that can be handled by this
      actor.
     */
    virtual const QList<MVerticalLevelType> supportedLevelTypes() = 0;

    /**
      Needs to be implemented in derived classes.

      Returns a pointer to a new @ref MNWPActorVariable instance.
     */
    virtual MNWPActorVariable* createActorVariable(
            const MSelectableDataSource& dataSource) = 0;

    /**
      Overload for @ref createActorVariable() with explicit parameters.
     */
    MNWPActorVariable* createActorVariable2(
            const QString& dataSourceID,
            const MVerticalLevelType levelType,
            const QString& variableName);

    /**
      Asks the user for a data source and adds a new actor variable for the
      data source. If the creation is successfull the new variable is
      returned (otherwise a nullptr is returned).

      @see createActorVariable()
     */
    MNWPActorVariable* addActorVariable();

    /**
      Adds an already existing actor variable to the actor. If @p syncName
      is a valid synchronization ID the variable is synchronized with the
      corresponding sync control.
     */
    MNWPActorVariable* addActorVariable(MNWPActorVariable* var,
                                        const QString& syncName);

    QString getSettingsID() override { return "NWPMultiVarActor"; }

    virtual void saveConfiguration(QSettings *settings);

    virtual void loadConfiguration(QSettings *settings);

    /**
      Broadcast a property change to all actor variables in this actor.

      @see MRequestProperties::actorPropertyChangeEvent()
     */
    void broadcastPropertyChangedEvent(
            MPropertyType::ChangeNotification ptype, void *value);

    bool isConnectedTo(MActor *actor) override;

protected:
    friend class MNWPActorVariable;

    /**
      The @ref MNWPMultiVarActor implementation of the @c
      initializeActorResources() method calls the initializeActorResources()
      methods of the registered @ref NWPActorVariable instances. This method
      has to be called from derived classes.
      */
    void initializeActorResources();

    void onQtPropertyChanged(QtProperty *property);

    /**
      This function is called whenever a new data field has been made current.
      Override it in derived classes to react to changing data fields.
      */
    virtual void dataFieldChangedEvent() { }

    /**
      Override this method in derived classes if the class needs to react to
      the deletion of a variable (e.g. to update lists of variable names).
      The method is called just before the variable is deleted.
     */
    virtual void onDeleteActorVariable(MNWPActorVariable* var)
    { Q_UNUSED(var); }

    /**
      Same as @ref onDeleteActorVariable() but called just after a new
      variable has been added.
     */
    virtual void onAddActorVariable(MNWPActorVariable* var)
    { Q_UNUSED(var); }

    /**
      Same as @ref onDeleteActorVariable() but called just after a variable has
      been changed.
     */
    virtual void onChangeActorVariable(MNWPActorVariable *var)
    { Q_UNUSED(var); }

    /** List of NWP variables that are rendered in this actor. */
    QList<MNWPActorVariable*> variables;

    MAnalysisControl *analysisControl;

    QtProperty *addVariableProperty;

    // Properties for the property browser.
    QtProperty *variablePropertiesGroup;
};

} // namespace Met3D

#endif // NWPMULTIVARACTOR_H

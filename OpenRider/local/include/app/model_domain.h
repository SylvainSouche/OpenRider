/// @file
/// The abstraction AppController is built from: domains of the model.
#pragma once

#include "app/intents.h"

namespace app {

/// A domain of the application model: the intents about one part of it
/// (the outing library, the list query, the selected outing, segments...)
/// and the background work behind it. Domains are the only code changing
/// the model; AppController owns one of each and routes every intent to the
/// domain handling its kind. A domain changes only its part of the model:
/// what another domain must do follows as an intent.
class ModelDomain {
public:
    virtual ~ModelDomain();

    /// The domain handles intents of `kind`.
    virtual bool Handles(IntentKind kind) const = 0;

    /// Handles an intent of a kind the domain handles.
    /// @param intent the intent
    /// @param out receives follow-up intents
    virtual void Handle(const Intent &intent, Intents &out) = 0;

    /// Once per frame: moves background progress into the model. Does
    /// nothing by default.
    /// @param out receives follow-up intents
    virtual void Tick(Intents &out);

    /// Stops background work, before the input closes. Does nothing by
    /// default.
    virtual void Shutdown();
};

} // namespace app

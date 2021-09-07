#if 0
LX200-based Omegon EQ500X Equatorial Mount

Copyright (C) 2019 Eric Dejouhanet (eric.dejouhanet@gmail.com)

This library is free software;
you can redistribute it and / or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation;
either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY;
without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library;
if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301  USA
#endif

#ifndef EQ500X_H
#define EQ500X_H

class EQ500X : public LX200Generic
{
    public:
        class MechanicalPoint
        {
            public:
                MechanicalPoint(double, double);
                    MechanicalPoint() {}
                public:
                    bool atParkingPosition() const;
                public:
                    double RAm() const;
                    double DECm() const;
                    double RAm(double const);
                    double DECm(double const);
                public:
                    double RAsky() const;
                    double DECsky() const;
                    double RAsky(double const);
                    double DECsky(double const);
                public:
                    enum PointingState { POINTING_NORMAL, POINTING_BEYOND_POLE };
                    enum PointingState setPointingState(enum PointingState);
                    enum PointingState getPointingState() const;
                public:
                    bool parseStringDEC(char const *, size_t);
                    bool parseStringRA(char const *, size_t);
                    char const * toStringDEC(char *, size_t) const;
                    char const * toStringRA(char *, size_t) const;
                public:
                    char const * toStringDEC_Sim(char *, size_t) const;
                public:
                    double RA_degrees_to(MechanicalPoint const &) const;
                    double DEC_degrees_to(MechanicalPoint const &) const;
                public:
                    double operator -(MechanicalPoint const &) const;
                    bool operator !=(MechanicalPoint const &) const;
                    bool operator ==(MechanicalPoint const &) const;
                protected:
                    enum PointingState _pointingState {POINTING_NORMAL};
                    long _RAm {0 * 3600}, _DECm {0 * 3600};
            };
        public:
            EQ500X();
            const char *getDefautName();
        protected:
            bool getCurrentMechanicalPosition(MechanicalPoint &);
            bool setTargetMechanicalPosition(MechanicalPoint const &);
            bool gotoTargetPosition(MechanicalPoint const &);
        protected:
            bool isParked();
            virtual double getLST();
            void resetSimulation();
        protected:
            int sendCmd(char const *);
            int getReply(char *, size_t const);
        protected:
            virtual bool checkConnection() override;
            virtual bool updateLocation(double, double, double) override;
            virtual void getBasicData() override;
            virtual bool ReadScopeStatus() override;
            virtual bool initProperties() override;
            virtual bool Goto(double, double) override;
            virtual bool Sync(double, double) override;
            virtual bool Abort() override;
            virtual void setPierSide(TelescopePierSide);
            virtual bool MoveNS(INDI_DIR_NS, TelescopeMotionCommand) override;
        private:
            MechanicalPoint currentMechPosition, targetMechPosition;
            double previousRA = {0}, previousDEC = {0};
            int countdown {0};
            // Movement markers, adjustment is done when no movement is required and all flags are cleared
            bool RAmIncrease {false}, RAmDecrease {false};
            bool DECmIncrease {false}, DECmDecrease {false};
            // Current adjustment rate
            struct _adjustment const * adjustment
            {
                nullptr
            };
            bool _gotoEngaged {false};
            enum TelescopeSlewRate savedSlewRateIndex {SLEW_MAX};
    };

#endif // EQ500X_H

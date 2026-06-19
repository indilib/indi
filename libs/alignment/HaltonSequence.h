/// \file HaltonSequence.h
/// \brief Generic mathematical utility for generating 2-D Halton quasi-random sequences.
///
/// The Halton sequence is a low-discrepancy (quasi-random) sequence that
/// provides a more uniform celestial distribution than pure random sampling
/// by avoiding clusters and gaps.
///
/// This refactored version is a pure mathematical utility: it provides raw values
/// in the range [0, 1) and delegates unit mapping (like RA/Dec) to the caller.
///
/// https://en.wikipedia.org/wiki/Halton_sequence

#pragma once

#include <cmath>
#include <utility>

namespace INDI
{
namespace AlignmentSubsystem
{

/// \brief Generic 2-D Halton quasi-random generator.
class HaltonSequence
{
    public:
        /// \brief Construct a generator with specified prime bases.
        /// \param baseX Prime base for the first dimension (default: 2).
        /// \param baseY Prime base for the second dimension (default: 3).
        HaltonSequence(int baseX = 2, int baseY = 3) : m_baseX(baseX), m_baseY(baseY) {}

        /// \brief Compute the i-th raw Halton term in the given base.
        /// \param i 1-indexed position in the sequence.
        /// \param base Prime base.
        /// \return Value in [0, 1).
        static double term(int i, int base)
        {
            double result = 0.0;
            double f      = 1.0;
            while (i > 0)
            {
                f /= static_cast<double>(base);
                result += (i % base) * f;
                i /= base;
            }
            return result;
        }

        /// \brief Get the n-th 2-D point in the sequence.
        /// \param n 1-indexed point index.
        /// \return A pair of values in the range [0, 1).
        std::pair<double, double> getRaw(int n) const
        {
            return {term(n, m_baseX), term(n, m_baseY)};
        }

        /// \brief Get the n-th value of the individual X dimension.
        double getX(int n) const
        {
            return term(n, m_baseX);
        }

        /// \brief Get the n-th value of the individual Y dimension.
        double getY(int n) const
        {
            return term(n, m_baseY);
        }

    private:
        int m_baseX;
        int m_baseY;
};

} // namespace AlignmentSubsystem
} // namespace INDI

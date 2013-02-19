/******************************************************************************
 *
 * Module Name: nsprepkg - Validation of package objects for predefined names
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2013, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#include "acpi.h"
#include "accommon.h"
#include "acnamesp.h"
#include "acpredef.h"


#define _COMPONENT          ACPI_NAMESPACE
        ACPI_MODULE_NAME    ("nsprepkg")


/* Local prototypes */

static ACPI_STATUS
AcpiNsCheckPackageList (
    ACPI_PREDEFINED_DATA        *Data,
    const ACPI_PREDEFINED_INFO  *Package,
    ACPI_OPERAND_OBJECT         **Elements,
    UINT32                      Count);

static ACPI_STATUS
AcpiNsCheckPackageElements (
    ACPI_PREDEFINED_DATA        *Data,
    ACPI_OPERAND_OBJECT         **Elements,
    UINT8                       Type1,
    UINT32                      Count1,
    UINT8                       Type2,
    UINT32                      Count2,
    UINT32                      StartIndex);


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsCheckPackage
 *
 * PARAMETERS:  Data                - Pointer to validation data structure
 *              ReturnObjectPtr     - Pointer to the object returned from the
 *                                    evaluation of a method or object
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Check a returned package object for the correct count and
 *              correct type of all sub-objects.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsCheckPackage (
    ACPI_PREDEFINED_DATA        *Data,
    ACPI_OPERAND_OBJECT         **ReturnObjectPtr)
{
    ACPI_OPERAND_OBJECT         *ReturnObject = *ReturnObjectPtr;
    const ACPI_PREDEFINED_INFO  *Package;
    ACPI_OPERAND_OBJECT         **Elements;
    ACPI_STATUS                 Status = AE_OK;
    UINT32                      ExpectedCount;
    UINT32                      Count;
    UINT32                      i;


    ACPI_FUNCTION_NAME (NsCheckPackage);


    /* The package info for this name is in the next table entry */

    Package = Data->Predefined + 1;

    ACPI_DEBUG_PRINT ((ACPI_DB_NAMES,
        "%s Validating return Package of Type %X, Count %X\n",
        Data->Pathname, Package->RetInfo.Type, ReturnObject->Package.Count));

    /*
     * For variable-length Packages, we can safely remove all embedded
     * and trailing NULL package elements
     */
    AcpiNsRemoveNullElements (Data, Package->RetInfo.Type, ReturnObject);

    /* Extract package count and elements array */

    Elements = ReturnObject->Package.Elements;
    Count = ReturnObject->Package.Count;

    /* The package must have at least one element, else invalid */

    if (!Count)
    {
        ACPI_WARN_PREDEFINED ((AE_INFO, Data->Pathname, Data->NodeFlags,
            "Return Package has no elements (empty)"));

        return (AE_AML_OPERAND_VALUE);
    }

    /*
     * Decode the type of the expected package contents
     *
     * PTYPE1 packages contain no subpackages
     * PTYPE2 packages contain sub-packages
     */
    switch (Package->RetInfo.Type)
    {
    case ACPI_PTYPE1_FIXED:

        /*
         * The package count is fixed and there are no sub-packages
         *
         * If package is too small, exit.
         * If package is larger than expected, issue warning but continue
         */
        ExpectedCount = Package->RetInfo.Count1 + Package->RetInfo.Count2;
        if (Count < ExpectedCount)
        {
            goto PackageTooSmall;
        }
        else if (Count > ExpectedCount)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_REPAIR,
                "%s: Return Package is larger than needed - "
                "found %u, expected %u\n",
                Data->Pathname, Count, ExpectedCount));
        }

        /* Validate all elements of the returned package */

        Status = AcpiNsCheckPackageElements (Data, Elements,
                    Package->RetInfo.ObjectType1, Package->RetInfo.Count1,
                    Package->RetInfo.ObjectType2, Package->RetInfo.Count2, 0);
        break;


    case ACPI_PTYPE1_VAR:

        /*
         * The package count is variable, there are no sub-packages, and all
         * elements must be of the same type
         */
        for (i = 0; i < Count; i++)
        {
            Status = AcpiNsCheckObjectType (Data, Elements,
                        Package->RetInfo.ObjectType1, i);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }
            Elements++;
        }
        break;


    case ACPI_PTYPE1_OPTION:

        /*
         * The package count is variable, there are no sub-packages. There are
         * a fixed number of required elements, and a variable number of
         * optional elements.
         *
         * Check if package is at least as large as the minimum required
         */
        ExpectedCount = Package->RetInfo3.Count;
        if (Count < ExpectedCount)
        {
            goto PackageTooSmall;
        }

        /* Variable number of sub-objects */

        for (i = 0; i < Count; i++)
        {
            if (i < Package->RetInfo3.Count)
            {
                /* These are the required package elements (0, 1, or 2) */

                Status = AcpiNsCheckObjectType (Data, Elements,
                            Package->RetInfo3.ObjectType[i], i);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }
            }
            else
            {
                /* These are the optional package elements */

                Status = AcpiNsCheckObjectType (Data, Elements,
                            Package->RetInfo3.TailObjectType, i);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }
            }
            Elements++;
        }
        break;


    case ACPI_PTYPE2_REV_FIXED:

        /* First element is the (Integer) revision */

        Status = AcpiNsCheckObjectType (Data, Elements,
                    ACPI_RTYPE_INTEGER, 0);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        Elements++;
        Count--;

        /* Examine the sub-packages */

        Status = AcpiNsCheckPackageList (Data, Package, Elements, Count);
        break;


    case ACPI_PTYPE2_PKG_COUNT:

        /* First element is the (Integer) count of sub-packages to follow */

        Status = AcpiNsCheckObjectType (Data, Elements,
                    ACPI_RTYPE_INTEGER, 0);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        /*
         * Count cannot be larger than the parent package length, but allow it
         * to be smaller. The >= accounts for the Integer above.
         */
        ExpectedCount = (UINT32) (*Elements)->Integer.Value;
        if (ExpectedCount >= Count)
        {
            goto PackageTooSmall;
        }

        Count = ExpectedCount;
        Elements++;

        /* Examine the sub-packages */

        Status = AcpiNsCheckPackageList (Data, Package, Elements, Count);
        break;


    case ACPI_PTYPE2:
    case ACPI_PTYPE2_FIXED:
    case ACPI_PTYPE2_MIN:
    case ACPI_PTYPE2_COUNT:
    case ACPI_PTYPE2_FIX_VAR:

        /*
         * These types all return a single Package that consists of a
         * variable number of sub-Packages.
         *
         * First, ensure that the first element is a sub-Package. If not,
         * the BIOS may have incorrectly returned the object as a single
         * package instead of a Package of Packages (a common error if
         * there is only one entry). We may be able to repair this by
         * wrapping the returned Package with a new outer Package.
         */
        if (*Elements && ((*Elements)->Common.Type != ACPI_TYPE_PACKAGE))
        {
            /* Create the new outer package and populate it */

            Status = AcpiNsWrapWithPackage (Data, ReturnObject, ReturnObjectPtr);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            /* Update locals to point to the new package (of 1 element) */

            ReturnObject = *ReturnObjectPtr;
            Elements = ReturnObject->Package.Elements;
            Count = 1;
        }

        /* Examine the sub-packages */

        Status = AcpiNsCheckPackageList (Data, Package, Elements, Count);
        break;


    default:

        /* Should not get here if predefined info table is correct */

        ACPI_WARN_PREDEFINED ((AE_INFO, Data->Pathname, Data->NodeFlags,
            "Invalid internal return type in table entry: %X",
            Package->RetInfo.Type));

        return (AE_AML_INTERNAL);
    }

    return (Status);


PackageTooSmall:

    /* Error exit for the case with an incorrect package count */

    ACPI_WARN_PREDEFINED ((AE_INFO, Data->Pathname, Data->NodeFlags,
        "Return Package is too small - found %u elements, expected %u",
        Count, ExpectedCount));

    return (AE_AML_OPERAND_VALUE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsCheckPackageList
 *
 * PARAMETERS:  Data            - Pointer to validation data structure
 *              Package         - Pointer to package-specific info for method
 *              Elements        - Element list of parent package. All elements
 *                                of this list should be of type Package.
 *              Count           - Count of subpackages
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Examine a list of subpackages
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiNsCheckPackageList (
    ACPI_PREDEFINED_DATA        *Data,
    const ACPI_PREDEFINED_INFO  *Package,
    ACPI_OPERAND_OBJECT         **Elements,
    UINT32                      Count)
{
    ACPI_OPERAND_OBJECT         *SubPackage;
    ACPI_OPERAND_OBJECT         **SubElements;
    ACPI_STATUS                 Status;
    UINT32                      ExpectedCount;
    UINT32                      i;
    UINT32                      j;


    /*
     * Validate each sub-Package in the parent Package
     *
     * NOTE: assumes list of sub-packages contains no NULL elements.
     * Any NULL elements should have been removed by earlier call
     * to AcpiNsRemoveNullElements.
     */
    for (i = 0; i < Count; i++)
    {
        SubPackage = *Elements;
        SubElements = SubPackage->Package.Elements;
        Data->ParentPackage = SubPackage;

        /* Each sub-object must be of type Package */

        Status = AcpiNsCheckObjectType (Data, &SubPackage,
                    ACPI_RTYPE_PACKAGE, i);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        /* Examine the different types of expected sub-packages */

        Data->ParentPackage = SubPackage;
        switch (Package->RetInfo.Type)
        {
        case ACPI_PTYPE2:
        case ACPI_PTYPE2_PKG_COUNT:
        case ACPI_PTYPE2_REV_FIXED:

            /* Each subpackage has a fixed number of elements */

            ExpectedCount = Package->RetInfo.Count1 + Package->RetInfo.Count2;
            if (SubPackage->Package.Count < ExpectedCount)
            {
                goto PackageTooSmall;
            }

            Status = AcpiNsCheckPackageElements (Data, SubElements,
                        Package->RetInfo.ObjectType1,
                        Package->RetInfo.Count1,
                        Package->RetInfo.ObjectType2,
                        Package->RetInfo.Count2, 0);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }
            break;


        case ACPI_PTYPE2_FIX_VAR:
            /*
             * Each subpackage has a fixed number of elements and an
             * optional element
             */
            ExpectedCount = Package->RetInfo.Count1 + Package->RetInfo.Count2;
            if (SubPackage->Package.Count < ExpectedCount)
            {
                goto PackageTooSmall;
            }

            Status = AcpiNsCheckPackageElements (Data, SubElements,
                        Package->RetInfo.ObjectType1,
                        Package->RetInfo.Count1,
                        Package->RetInfo.ObjectType2,
                        SubPackage->Package.Count - Package->RetInfo.Count1, 0);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }
            break;


        case ACPI_PTYPE2_FIXED:

            /* Each sub-package has a fixed length */

            ExpectedCount = Package->RetInfo2.Count;
            if (SubPackage->Package.Count < ExpectedCount)
            {
                goto PackageTooSmall;
            }

            /* Check the type of each sub-package element */

            for (j = 0; j < ExpectedCount; j++)
            {
                Status = AcpiNsCheckObjectType (Data, &SubElements[j],
                            Package->RetInfo2.ObjectType[j], j);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }
            }
            break;


        case ACPI_PTYPE2_MIN:

            /* Each sub-package has a variable but minimum length */

            ExpectedCount = Package->RetInfo.Count1;
            if (SubPackage->Package.Count < ExpectedCount)
            {
                goto PackageTooSmall;
            }

            /* Check the type of each sub-package element */

            Status = AcpiNsCheckPackageElements (Data, SubElements,
                        Package->RetInfo.ObjectType1,
                        SubPackage->Package.Count, 0, 0, 0);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }
            break;


        case ACPI_PTYPE2_COUNT:

            /*
             * First element is the (Integer) count of elements, including
             * the count field (the ACPI name is NumElements)
             */
            Status = AcpiNsCheckObjectType (Data, SubElements,
                        ACPI_RTYPE_INTEGER, 0);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            /*
             * Make sure package is large enough for the Count and is
             * is as large as the minimum size
             */
            ExpectedCount = (UINT32) (*SubElements)->Integer.Value;
            if (SubPackage->Package.Count < ExpectedCount)
            {
                goto PackageTooSmall;
            }
            if (SubPackage->Package.Count < Package->RetInfo.Count1)
            {
                ExpectedCount = Package->RetInfo.Count1;
                goto PackageTooSmall;
            }
            if (ExpectedCount == 0)
            {
                /*
                 * Either the NumEntries element was originally zero or it was
                 * a NULL element and repaired to an Integer of value zero.
                 * In either case, repair it by setting NumEntries to be the
                 * actual size of the subpackage.
                 */
                ExpectedCount = SubPackage->Package.Count;
                (*SubElements)->Integer.Value = ExpectedCount;
            }

            /* Check the type of each sub-package element */

            Status = AcpiNsCheckPackageElements (Data, (SubElements + 1),
                        Package->RetInfo.ObjectType1,
                        (ExpectedCount - 1), 0, 0, 1);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }
            break;


        default: /* Should not get here, type was validated by caller */

            return (AE_AML_INTERNAL);
        }

        Elements++;
    }

    return (AE_OK);


PackageTooSmall:

    /* The sub-package count was smaller than required */

    ACPI_WARN_PREDEFINED ((AE_INFO, Data->Pathname, Data->NodeFlags,
        "Return Sub-Package[%u] is too small - found %u elements, expected %u",
        i, SubPackage->Package.Count, ExpectedCount));

    return (AE_AML_OPERAND_VALUE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsCheckPackageElements
 *
 * PARAMETERS:  Data            - Pointer to validation data structure
 *              Elements        - Pointer to the package elements array
 *              Type1           - Object type for first group
 *              Count1          - Count for first group
 *              Type2           - Object type for second group
 *              Count2          - Count for second group
 *              StartIndex      - Start of the first group of elements
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Check that all elements of a package are of the correct object
 *              type. Supports up to two groups of different object types.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiNsCheckPackageElements (
    ACPI_PREDEFINED_DATA        *Data,
    ACPI_OPERAND_OBJECT         **Elements,
    UINT8                       Type1,
    UINT32                      Count1,
    UINT8                       Type2,
    UINT32                      Count2,
    UINT32                      StartIndex)
{
    ACPI_OPERAND_OBJECT         **ThisElement = Elements;
    ACPI_STATUS                 Status;
    UINT32                      i;


    /*
     * Up to two groups of package elements are supported by the data
     * structure. All elements in each group must be of the same type.
     * The second group can have a count of zero.
     */
    for (i = 0; i < Count1; i++)
    {
        Status = AcpiNsCheckObjectType (Data, ThisElement,
                    Type1, i + StartIndex);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
        ThisElement++;
    }

    for (i = 0; i < Count2; i++)
    {
        Status = AcpiNsCheckObjectType (Data, ThisElement,
                    Type2, (i + Count1 + StartIndex));
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
        ThisElement++;
    }

    return (AE_OK);
}